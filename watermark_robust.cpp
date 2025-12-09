#include "watermark_robust.h"
#ifndef NO_OPENCV
#include <vector>
#include <random>
#include <cstring>
#include <algorithm>
#include <iostream>

static const int WATERMARK_LEN = 15;               // 15 字符
static const int PAYLOAD_BITS = WATERMARK_LEN * 8; // 120 bits
static const unsigned int FIXED_SEED = 0xC0FFEE;   // 固定种子，盲提取
static const int BLOCK = 8;                        // DCT 块尺寸 8x8
static const float DELTA = 16.0f;                  // QIM 步长（可调整）
static const int REPEAT = 11;                      // 每个 bit 重复嵌入次数（冗余）
static const int SYNC_LEN = 16;                    // 同步头字节长度（16 bytes = 128 bits）

// 同步模板（16 字节固定值）——你也可以换成随机但固定的值
static const unsigned char SYNC_PATTERN[SYNC_LEN] = {
    0xA3, 0x5F, 0xC1, 0x7D, 0x2B, 0x9E, 0x11, 0xF0,
    0x4C, 0xD2, 0x88, 0xE7, 0x33, 0x19, 0xBE, 0x42};

// 将字符串填充/截断到 15 字符
static std::string fixedText(const std::string &in)
{
    std::string t = in;
    if (t.size() < WATERMARK_LEN)
        t.append(WATERMARK_LEN - t.size(), '0');
    if (t.size() > WATERMARK_LEN)
        t = t.substr(0, WATERMARK_LEN);
    return t;
}

// 字符串 -> bits (msb first per byte)
static void strToBits(const std::vector<unsigned char> &bytes, std::vector<int> &bits)
{
    bits.clear();
    for (unsigned char c : bytes)
    {
        for (int i = 7; i >= 0; --i)
            bits.push_back((c >> i) & 1);
    }
}

// bits -> bytes
static std::vector<unsigned char> bitsToBytes(const std::vector<int> &bits)
{
    std::vector<unsigned char> out;
    int n = bits.size() / 8;
    out.reserve(n);
    for (int i = 0; i < n; ++i)
    {
        unsigned char c = 0;
        for (int b = 0; b < 8; ++b)
        {
            c = (c << 1) | (bits[i * 8 + b] & 1);
        }
        out.push_back(c);
    }
    return out;
}

// 计算简单校验（8-bit XOR）
static unsigned char simpleChecksum(const std::vector<unsigned char> &data)
{
    unsigned char c = 0;
    for (auto b : data)
        c ^= b;
    return c;
}

// Helper: break Y channel into 8x8 blocks, apply DCT, apply inverse DCT and write back
// We'll operate on the Y channel float matrix
// Embed single bit into a specific block by modifying a chosen mid-frequency coefficient index (u,v)
static void embedBitInBlock(cv::Mat &block8f, int bit, float delta, int coef_u = 2, int coef_v = 3)
{
    // block8f is 8x8 CV_32F DCT coefficients (in place)
    float &coeff = block8f.at<float>(coef_u, coef_v);
    float sign = (coeff >= 0.0f) ? 1.0f : -1.0f;
    float absval = std::abs(coeff);
    // QIM: quantize absval/delta, force parity to bit
    int q = static_cast<int>(std::floor(absval / delta + 0.5f));
    // set parity of q to bit
    if ((q & 1) != bit)
    {
        // adjust q by +-1 to change parity
        if (q == 0)
            q = 1;
        else
            q = q + ((q + 1) % 2 == bit ? 1 : -1);
    }
    float newAbs = q * delta + delta * 0.35f; // small offset to avoid exact quant boundary
    coeff = sign * newAbs;
}

// Extract bit from block by reading coefficient parity after quantization
static int extractBitFromBlock(const cv::Mat &block8f, float delta, int coef_u = 2, int coef_v = 3)
{
    float coeff = block8f.at<float>(coef_u, coef_v);
    float absval = std::abs(coeff);
    int q = static_cast<int>(std::floor(absval / delta + 0.5f));
    return q & 1;
}

// Main embed function
bool embedRobustWatermark(cv::Mat &image, const std::string &textInput)
{
    if (image.empty())
        return false;
    // preserve alpha if present
    cv::Mat alpha;
    cv::Mat work;
    if (image.channels() == 4)
    {
        std::vector<cv::Mat> ch;
        cv::split(image, ch);
        alpha = ch[3].clone();
        cv::merge(std::vector<cv::Mat>{ch[0], ch[1], ch[2]}, work);
    }
    else
    {
        work = image.clone();
    }
    cv::Mat imgBGR;
    if (work.channels() == 3)
        imgBGR = work;
    else if (work.channels() == 1)
        cv::cvtColor(work, imgBGR, cv::COLOR_GRAY2BGR);
    else
        return false;

    // convert to YCrCb and split
    cv::Mat imgYCrCb;
    cv::cvtColor(imgBGR, imgYCrCb, cv::COLOR_BGR2YCrCb);
    std::vector<cv::Mat> planes;
    cv::split(imgYCrCb, planes);
    cv::Mat Y = planes[0]; // CV_8U

    int rows = Y.rows, cols = Y.cols;
    int blockRows = rows / BLOCK, blockCols = cols / BLOCK;
    int totalBlocks = blockRows * blockCols;
    if (totalBlocks <= 0)
        return false;

    // compose payload: SYNC + payload + checksum
    std::vector<unsigned char> payloadBytes;
    payloadBytes.insert(payloadBytes.end(), SYNC_PATTERN, SYNC_PATTERN + SYNC_LEN);

    std::string fixed = fixedText(textInput);
    for (char c : fixed)
        payloadBytes.push_back(static_cast<unsigned char>(c));
    // add checksum byte
    unsigned char cs = simpleChecksum(payloadBytes);
    payloadBytes.push_back(cs);

    // bits to embed
    std::vector<int> payloadBits;
    strToBits(payloadBytes, payloadBits);
    int totalBitsToEmbed = payloadBits.size(); // SYNC + payload + checksum bits

    // We'll embed each bit REPEAT times into different random blocks
    if (totalBlocks < REPEAT)
    {
        // image too small to provide repeats
        return false;
    }

    // choose coefficient position in block (mid frequency)
    int coef_u = 2, coef_v = 3;

    // prepare float Y for DCT processing
    cv::Mat Yf;
    Y.convertTo(Yf, CV_32F);

    // generate randomized sequence of block indices
    std::mt19937 rng(FIXED_SEED);
    std::uniform_int_distribution<int> distBlock(0, totalBlocks - 1);

    // For stable embedding, we avoid embedding multiple bits into the exact same block position frequently.
    // We'll generate an array of chosen block indices for each embedding (totalBitsToEmbed * REPEAT)
    std::vector<int> chosenBlocks;
    chosenBlocks.reserve(totalBitsToEmbed * REPEAT);
    for (int i = 0; i < totalBitsToEmbed * REPEAT; ++i)
    {
        chosenBlocks.push_back(distBlock(rng));
    }

    // For each block, we need to extract 8x8, do DCT, modify, inverse DCT.
    // We'll process blocks in a loop; because multiple embeddings can hit same block, accumulate changes by
    // re-dct/re-idct for each time (simple approach).
    for (int bitIdx = 0; bitIdx < totalBitsToEmbed; ++bitIdx)
    {
        int bit = payloadBits[bitIdx];
        // embed this bit REPEAT times
        for (int r = 0; r < REPEAT; ++r)
        {
            int chosen = chosenBlocks[bitIdx * REPEAT + r];
            int br = chosen / blockCols;
            int bc = chosen % blockCols;
            int y0 = br * BLOCK;
            int x0 = bc * BLOCK;
            // extract 8x8 block
            cv::Mat block = cv::Mat::zeros(BLOCK, BLOCK, CV_32F);
            for (int i = 0; i < BLOCK; ++i)
                for (int j = 0; j < BLOCK; ++j)
                    block.at<float>(i, j) = Yf.at<float>(y0 + i, x0 + j);

            // forward DCT
            cv::Mat dctBlock;
            cv::dct(block, dctBlock);

            // embed bit
            embedBitInBlock(dctBlock, bit, DELTA, coef_u, coef_v);

            // inverse DCT
            cv::Mat idctBlock;
            cv::idct(dctBlock, idctBlock);

            // write back (clamp)
            for (int i = 0; i < BLOCK; ++i)
            {
                for (int j = 0; j < BLOCK; ++j)
                {
                    float v = idctBlock.at<float>(i, j);
                    // keep within valid 0-255
                    if (v < 0.0f)
                        v = 0.0f;
                    if (v > 255.0f)
                        v = 255.0f;
                    Yf.at<float>(y0 + i, x0 + j) = v;
                }
            }
        }
    }

    // convert Yf back to 8U
    cv::Mat Yout;
    Yf.convertTo(Yout, CV_8U);

    // merge back and convert to BGR
    planes[0] = Yout;
    cv::Mat merged;
    cv::merge(planes, imgYCrCb);
    cv::cvtColor(imgYCrCb, imgBGR, cv::COLOR_YCrCb2BGR);

    // restore alpha if needed and write to image
    if (!alpha.empty())
    {
        std::vector<cv::Mat> outch;
        cv::split(imgBGR, outch);
        outch.push_back(alpha);
        cv::merge(outch, image);
    }
    else
    {
        image = imgBGR;
    }

    return true;
}

// Extraction
bool extractRobustWatermark(const cv::Mat &image, std::string &outText)
{
    if (image.empty())
        return false;
    // separate alpha if present
    cv::Mat work;
    if (image.channels() == 4)
    {
        std::vector<cv::Mat> ch;
        cv::split(image, ch);
        cv::merge(std::vector<cv::Mat>{ch[0], ch[1], ch[2]}, work);
    }
    else
    {
        work = image.clone();
    }

    cv::Mat imgBGR;
    if (work.channels() == 3)
        imgBGR = work;
    else if (work.channels() == 1)
        cv::cvtColor(work, imgBGR, cv::COLOR_GRAY2BGR);
    else
        return false;

    cv::Mat imgYCrCb;
    cv::cvtColor(imgBGR, imgYCrCb, cv::COLOR_BGR2YCrCb);
    std::vector<cv::Mat> planes;
    cv::split(imgYCrCb, planes);
    cv::Mat Y = planes[0];

    int rows = Y.rows, cols = Y.cols;
    int blockRows = rows / BLOCK, blockCols = cols / BLOCK;
    int totalBlocks = blockRows * blockCols;
    if (totalBlocks <= 0)
        return false;

    // We'll recover the same payloadBits length as embed
    int expectedPayloadBytes = SYNC_LEN + WATERMARK_LEN + 1; // sync + 15 bytes + checksum
    int expectedBits = expectedPayloadBytes * 8;

    // prepare Y float
    cv::Mat Yf;
    Y.convertTo(Yf, CV_32F);

    // reproduce chosen block indices
    std::mt19937 rng(FIXED_SEED);
    std::uniform_int_distribution<int> distBlock(0, totalBlocks - 1);

    std::vector<int> chosenBlocks;
    chosenBlocks.reserve(expectedBits * REPEAT);
    for (int i = 0; i < expectedBits * REPEAT; ++i)
        chosenBlocks.push_back(distBlock(rng));

    int coef_u = 2, coef_v = 3;

    // collect votes for each bit
    std::vector<int> votes(expectedBits, 0);

    for (int bitIdx = 0; bitIdx < expectedBits; ++bitIdx)
    {
        for (int r = 0; r < REPEAT; ++r)
        {
            int chosen = chosenBlocks[bitIdx * REPEAT + r];
            int br = chosen / blockCols;
            int bc = chosen % blockCols;
            int y0 = br * BLOCK;
            int x0 = bc * BLOCK;

            cv::Mat block = cv::Mat::zeros(BLOCK, BLOCK, CV_32F);
            for (int i = 0; i < BLOCK; ++i)
                for (int j = 0; j < BLOCK; ++j)
                    block.at<float>(i, j) = Yf.at<float>(y0 + i, x0 + j);

            cv::Mat dctBlock;
            cv::dct(block, dctBlock);

            int bit = extractBitFromBlock(dctBlock, DELTA, coef_u, coef_v);
            votes[bitIdx] += (bit == 1) ? 1 : -1;
        }
    }

    // majority vote to produce recovered bits
    std::vector<int> recoveredBits(expectedBits, 0);
    for (int i = 0; i < expectedBits; ++i)
        recoveredBits[i] = (votes[i] > 0) ? 1 : 0;

    // convert to bytes
    std::vector<unsigned char> recBytes = bitsToBytes(recoveredBits);

    // verify sync exists at start
    if ((int)recBytes.size() < expectedPayloadBytes)
        return false;
    bool syncOk = true;
    for (int i = 0; i < SYNC_LEN; ++i)
    {
        if (recBytes[i] != SYNC_PATTERN[i])
        {
            syncOk = false;
            break;
        }
    }
    if (!syncOk)
    {
        // failed to find sync -> extraction failed
        return false;
    }

    // extract watermark bytes
    std::vector<unsigned char> wmBytes(recBytes.begin() + SYNC_LEN, recBytes.begin() + SYNC_LEN + WATERMARK_LEN);
    unsigned char extractedCS = recBytes[SYNC_LEN + WATERMARK_LEN];
    // compute checksum on sync+wm
    std::vector<unsigned char> checkVec;
    checkVec.insert(checkVec.end(), recBytes.begin(), recBytes.begin() + SYNC_LEN + WATERMARK_LEN);
    unsigned char cs = simpleChecksum(checkVec);
    if (cs != extractedCS)
    {
        // checksum mismatch -> maybe attack too strong
        return false;
    }

    // convert to string
    outText.clear();
    outText.reserve(WATERMARK_LEN);
    for (unsigned char c : wmBytes)
        outText.push_back(static_cast<char>(c));
    return true;
}
#endif // NO_OPENCV
