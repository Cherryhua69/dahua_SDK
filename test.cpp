#include <iostream>
#include <string>
#include <random>
#include <algorithm>
#include <json/json.h>
#include <cstdio>
#include <cstring>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include "dhnetsdk.h"
#include "play.h"

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
}
#pragma comment(lib, "dhnetsdk.lib")
#pragma comment(lib, "play.lib")

 
using namespace cv;
using namespace std;
 
static BOOL g_bNetSDKInitFlag = FALSE;
static LLONG g_lLoginHandle = 0L;
static LLONG g_lRealHandle = 0;
static char g_szDevIp[32] = "192.168.2.108";  // you can change me
static WORD g_nPort = 37777; // tcp连接端口，需与期望登录设备页面tcp端口配置一致
static char g_szUserName[64] = "admin";
static char g_szPasswd[64] = "s19871007";   // you can change me
LONG g_lRealPort = 0; // 全局播放库port号
 
// 常用回调集合声明
void CALLBACK DisConnectFunc(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser);
void CALLBACK HaveReConnect(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser);
void CALLBACK DecCBFun(LONG nPort, char * pBuf, LONG nSize, FRAME_INFO * pFrameInfo, void* pUserData,LONG nReserved2);
void CALLBACK CBDecode(LONG nPort, FRAME_DECODE_INFO* pFrameDecodeInfo, FRAME_INFO_EX* pFrameInfo, void* pUser);
void CALLBACK RealDataCallBackEx(LLONG lRealHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, LONG param, LDWORD dwUser);
void CALLBACK IVSInfoCallbackFunc(char* pIVSBuf, LONG nIVSType, LONG nIVSBufLen, LONG nFrameSeq, void* pReserved, void* pUserData);
 
std::string generate_random_string_fast(size_t length) {
    static const std::string charset = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    static std::random_device rd;
    static std::mt19937 generator(rd());
    static std::uniform_int_distribution<> distribution(0, charset.size() - 1);
    
    std::string result;
    result.resize(length);
    
    std::generate_n(result.begin(), length, []() {
        return charset[distribution(generator)];
    });
    
    return result;
}

void InitTest()
{
    // 初始化SDK
    g_bNetSDKInitFlag = CLIENT_Init(DisConnectFunc, 0);
    if (FALSE == g_bNetSDKInitFlag)
    {
        printf("Initialize client SDK fail; \n");
        return;
    }
    else
    {
        printf("Initialize client SDK done; \n");
    }
    // 获取SDK版本信息
    DWORD dwNetSdkVersion = CLIENT_GetSDKVersion();
    printf("NetSDK version is [%d]\n", dwNetSdkVersion);
    
    // 设置断线重连回调接口
    CLIENT_SetAutoReconnect(&HaveReConnect, 0);
    
    // 设置登录超时时间和尝试次数
    int nWaitTime = 5000;   // 登录请求响应超时时间设置为 5s
    int nTryTimes = 3;      // 登录时尝试建立链接3次
    CLIENT_SetConnectTime(nWaitTime, nTryTimes);
    
    // 设置更多网络参数
    NET_PARAM stuNetParm = {0};
    stuNetParm.nConnectTime = 3000; // 登录时尝试建立链接的超时时间
    CLIENT_SetNetworkParam(&stuNetParm);
    
    NET_IN_LOGIN_WITH_HIGHLEVEL_SECURITY stInparam;
    memset(&stInparam, 0, sizeof(stInparam));
    stInparam.dwSize = sizeof(stInparam);
    strncpy(stInparam.szIP, g_szDevIp, sizeof(stInparam.szIP) - 1);
    strncpy(stInparam.szPassword, g_szPasswd, sizeof(stInparam.szPassword) - 1);
    strncpy(stInparam.szUserName, g_szUserName, sizeof(stInparam.szUserName) - 1);
    stInparam.nPort = g_nPort;
    stInparam.emSpecCap = EM_LOGIN_SPEC_CAP_TCP;
    
    NET_OUT_LOGIN_WITH_HIGHLEVEL_SECURITY stOutparam;
    memset(&stOutparam, 0, sizeof(stOutparam));
    stOutparam.dwSize = sizeof(stOutparam);
    
    while(0 == g_lLoginHandle)
    {
        // 登录设备
        g_lLoginHandle = CLIENT_LoginWithHighLevelSecurity(&stInparam, &stOutparam);
        if (0 == g_lLoginHandle)
        {
            printf("CLIENT_LoginWithHighLevelSecurity %s[%d]Failed!Last Error[%x]\n", g_szDevIp, g_nPort, CLIENT_GetLastError());
        }
        else
        {
            printf("CLIENT_LoginWithHighLevelSecurity %s[%d] Success\n", g_szDevIp, g_nPort);
        }
     
        printf("\n");
    }
}
 
void RunTest()
{
    if (FALSE == g_bNetSDKInitFlag)
    {
        return;
    }
    if (0 == g_lLoginHandle)
    {
        return;
    }
    // printf("user can input any key to quit during real play data callback!\n");
    if (g_lLoginHandle != 0)
	{
        if(PLAY_GetFreePort(&g_lRealPort)){
            // 设置流模式，回放下必须设置为STREAME_FILE，实时流设置为STREAME_REALTIME
            if(PLAY_SetStreamOpenMode(g_lRealPort, STREAME_REALTIME)){
                // 初始化播放库
                if(PLAY_OpenStream(g_lRealPort, NULL, 0, 1920 * 1080))
                {
                    // 设置解码回调函数
                    // PLAYSDK_API BOOL CALLMETHOD PLAY_SetDecodeCallBack(LONG nPort, fCBDecode cbDec, void* pUser);
                    // PLAY_SetDecodeCallBack(g_lRealPort, CBDecode, 0) // 带有对应帧的编号，yuv的三个部分的图片数据
                    // PLAY_SetDecCallBackEx(g_lRealPort, DecCBFun, 0) 
                    if(PLAY_SetDecodeCallBack(g_lRealPort, CBDecode, 0)){
                        // 开始播放
                        if(PLAY_Play(g_lRealPort,NULL))
                        {
                            std::cout << "PLAY_Play success" << std::endl;
                        }else{
                            std::cout << "PLAY_Play failed, errorNo: " << PLAY_GetLastErrorEx() << std::endl;
                            //异常处理
                            PLAY_Stop(g_lRealPort);
                            PLAY_CloseStream(g_lRealPort);
                            return;
                        }
                    }else{
                        std::cout << "PLAY_SetDecCallBackEx  failed, errorNo: " << PLAY_GetLastErrorEx() << std::endl;
                        return;
                    }

                }else{
                    std::cout << "PLAY_OpenStream failed, errorNo: " << PLAY_GetLastErrorEx() << std::endl;
                    //异常处理
                    PLAY_CloseStream(g_lRealPort);
                    return;
                }
            }else{
                std::cout << "PLAY_SetStreamOpenMode failed, errorNo: " << PLAY_GetLastErrorEx() << std::endl;
                return;
            }
        }else{
            std::cout << "PLAY_GetFreePort failed, errorNo: " << PLAY_GetLastErrorEx() << std::endl;
            return;
        }

        //启动预览并设置回调数据流
        PLAY_RenderPrivateData(g_lRealPort, TRUE, 0); // 开启私有数据显示
        // IVSDRAWER_TRACK = 1,						//track
        // IVSDRAWER_ALARM,							//Event alarm
        // IVSDRAWER_RULE,								//rule
        // IVSDRAWER_TRACKEX2 = 14,					//trackEx2
        // IVSDRAWER_SMARTMOTION = 23,					//smart motion detection
        // IVSDRAWER_DATA_WITH_LARGE_AMOUNT = 25		//large amount (include CrowdDistri heatmap)
        // PLAY_SetIvsEnable(g_lRealPort, IVSDRAWER_ALARM, TRUE);
        // PLAY_SetIvsEnable(g_lRealPort, IVSDRAWER_ALARM, TRUE);
        // PLAY_SetIvsEnable(g_lRealPort, IVSDRAWER_ALARM, TRUE);
        PLAY_SetIvsEnable(g_lRealPort, IVSDRAWER_SMARTMOTION, TRUE);
        PLAY_SetIvsEnable(g_lRealPort, IVSDRAWER_RULE, TRUE); 
        PLAY_SetIvsEnable(g_lRealPort, IVSDRAWER_TRACKEX2, TRUE);
        PLAY_SetIvsEnable(g_lRealPort, IVSDRAWER_ALARM, TRUE);
        // PLAYSDK_API BOOL CALLMETHOD PLAY_SetIVSCallBack(LONG nPort, fIVSInfoCallbackFunc pFunc, void* pUserData);
        PLAY_SetIVSCallBack(g_lRealPort, &IVSInfoCallbackFunc, 0);
        // std::cout << "启动预览并设置回调数据流：g_lLoginHandle: " << g_lLoginHandle<< std::endl;
        long lRealHandle = CLIENT_RealPlayEx(g_lLoginHandle, 1, NULL);
        if(0 != lRealHandle)
        {
            printf("开始实时预览，通道: %d\n", 1);
            // 设置回调函数处理数据
            // DWORD dwFlag = 0x00000001;
            CLIENT_SetRealDataCallBackEx(lRealHandle, &RealDataCallBackEx, (DWORD)0, 0x1f);
        }
        else
        {
            printf("Fail to play!\n");
            PLAY_Stop(g_lRealPort);
            PLAY_CloseStream(g_lRealPort);
        }
	}

}
 
void EndTest()
{
    printf("input any key to quit!\n");
    getchar();
    
    // 关闭预览
    if (0 != g_lRealHandle)
    {
        if (FALSE == CLIENT_StopRealPlayEx(g_lRealHandle))
        {
            printf("CLIENT_StopRealPlayEx Failed, g_lRealHandle[%x]!Last Error[%x]\n", g_lRealHandle, CLIENT_GetLastError());
        }
        else
        {
            g_lRealHandle = 0;
        }
    }
    
    // 退出设备
    if (0 != g_lLoginHandle)
    {
        if(FALSE == CLIENT_Logout(g_lLoginHandle))
        {
            printf("CLIENT_Logout Failed!Last Error[%x]\n", CLIENT_GetLastError());
        }
        else
        {
            g_lLoginHandle = 0;
        }
    }
    
    // 清理初始化资源
    if (TRUE == g_bNetSDKInitFlag)
    {
        CLIENT_Cleanup();
        g_bNetSDKInitFlag = FALSE;
    }
}
 
// 常用回调集合定义
void CALLBACK DisConnectFunc(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser)
{
    printf("Call DisConnectFunc\n");
    printf("lLoginID[0x%x]", lLoginID);
    if (NULL != pchDVRIP)
    {
        printf("pchDVRIP[%s]\n", pchDVRIP);
    }
    printf("nDVRPort[%d]\n", nDVRPort);
    printf("dwUser[%p]\n", dwUser);
    printf("\n");
}
 
void CALLBACK HaveReConnect(LLONG lLoginID, char *pchDVRIP, LONG nDVRPort, LDWORD dwUser)
{
    printf("Call HaveReConnect\n");
    printf("lLoginID[0x%x]", lLoginID);
    if (NULL != pchDVRIP)
    {
        printf("pchDVRIP[%s]\n", pchDVRIP);
    }
    printf("nDVRPort[%d]\n", nDVRPort);
    printf("dwUser[%p]\n", dwUser);
    printf("\n");
}

//解码后数据回调，YUV数据格式为I420
void CALLBACK DecCBFun(LONG nPort, char * pBuf, LONG nSize, FRAME_INFO * pFrameInfo, void* pUserData, LONG nReserved2)
{
    if (pFrameInfo->nType == T_IYUV)
    {
        // std::cout << " DecCBFun, nWidth: " << pFrameInfo->nWidth << " nHeight:" << pFrameInfo->nHeight << std::endl;
        // 创建目标BGR Mat
        cv::Mat bgr(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC3);
        // 创建YUV420(I420)格式的Mat
        cv::Mat yuv420(pFrameInfo->nHeight + pFrameInfo->nHeight / 2, pFrameInfo->nWidth, CV_8UC1, (unsigned char*)pBuf);
        // 使用OpenCV进行颜色空间转换
        cv::cvtColor(yuv420, bgr, cv::COLOR_YUV2BGR_I420);
        yuv420.~Mat();
        // 图片保存测试
        std::string filename = generate_random_string_fast(10) + ".jpg";
        imwrite(filename, bgr);
    }
}

void CALLBACK CBDecode(LONG nPort, FRAME_DECODE_INFO* pFrameDecodeInfo, FRAME_INFO_EX* pFrameInfo, void* pUser){
    if (pFrameDecodeInfo->nType == T_IYUV)
    {
        // 验证输入参数
        if (!pFrameDecodeInfo->pVideoData[0] || !pFrameDecodeInfo->pVideoData[1] || !pFrameDecodeInfo->pVideoData[2] || pFrameInfo->nWidth <= 0 || pFrameInfo->nHeight <= 0) {
            return;
        }
        cout<<"nFrameSeq(对应的视频帧序号):"<<pFrameInfo->nFrameSeq<<endl;
        uint8_t* yData = static_cast<uint8_t*>(pFrameDecodeInfo->pVideoData[0]);
        uint8_t* uData = static_cast<uint8_t*>(pFrameDecodeInfo->pVideoData[1]);
        uint8_t* vData = static_cast<uint8_t*>(pFrameDecodeInfo->pVideoData[2]);
        // YUV420P格式：Y分量占width*height，U和V分量各占width*height/4
        int ySize = pFrameInfo->nWidth * pFrameInfo->nHeight;
        int uvSize = pFrameInfo->nWidth * pFrameInfo->nHeight / 4;
        // 创建连续的YUV420P数据缓冲区
        cv::Mat yuvData(pFrameInfo->nHeight + pFrameInfo->nHeight / 2, pFrameInfo->nWidth, CV_8UC1);
        // 复制Y分量到缓冲区前部
        memcpy(yuvData.data, yData, ySize);
        // 复制U分量
        memcpy(yuvData.data + ySize, uData, uvSize);
        // 复制V分量  
        memcpy(yuvData.data + ySize + uvSize, vData, uvSize);
        // 转换YUV420P到BGR
        cv::Mat bgrMat;
        cv::cvtColor(yuvData, bgrMat, cv::COLOR_YUV2BGR_I420);
        yuvData.~Mat();
        // 图片保存测试
        std::string filename = generate_random_string_fast(10) + ".jpg";
        imwrite(filename, bgrMat);
    }
}
 
void CALLBACK RealDataCallBackEx(LLONG lRealHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, LONG param, LDWORD dwUser)
{
    bool bInput = FALSE;
    if (0 != lRealHandle)
    {
        switch(dwDataType)
        {
            case 0:
                // 原始音视频混合数据
                bInput = PLAY_InputData(g_lRealPort,pBuffer,dwBufSize);
                if (!bInput)
                {
                    printf("PLAY_InputData error: %d\n", PLAY_GetLastError(g_lRealPort));
                }
                break;
            case 1:
            {
                // 标准视频数据 (H.264/H.265)
                break;
            }
            case 2:
            {
                // YUV数据
            }
            case 3:
                // PCM音频数据
                break;
            case 4:
                // 原始音频数据
                break;
            default:
                break;
        }
    }
}

// 安全解析 JSON 字符串
Json::Value stringDataToJson(char* pIVSBuf, LONG nIVSBufLen) {
    Json::Value root;
    if (pIVSBuf && nIVSBufLen > 0) {
        // // 方法1: 创建安全的字符串副本
        std::string safeData(pIVSBuf, nIVSBufLen);
        // root["data"] = safeData;
        // root["pointer_address"] = reinterpret_cast<uintptr_t>(pIVSBuf);
        // root["buffer_length"] = nIVSBufLen;
        
        // 方法2: 如果确定是JSON格式，直接解析
        try {
            Json::Reader reader;
            Json::Value jsonData;
            if (reader.parse(safeData, jsonData)) {
                root["parsed_data"] = jsonData;
            } else {
                root["raw_data"] = safeData;
                root["parse_error"] = "Invalid JSON format";
            }
        } catch (const std::exception& e) {
            root["raw_data"] = safeData;
            root["parse_error"] = e.what();
        }
    } else {
        root["error"] = "null_pointer_or_zero_length";
    }
    return root;
}

// 生成带时间戳的文件名
std::string generateFileName(LONG nFrameSeq) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << "ivs_data_" 
       << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
       << "_" << std::setfill('0') << std::setw(3) << ms.count()
       << "_frame_" << nFrameSeq
       << ".json";
    
    return ss.str();
}

// 方法2: 保存格式化的JSON数据
bool saveFormattedJsonToFile(const Json::Value& json, const std::string& filename) {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filename << std::endl;
            return false;
        }
        
        // 使用Json::StreamWriterBuilder获得更好的格式控制
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "    "; // 4个空格缩进
        builder["precision"] = 6;
        builder["precisionType"] = "significant";
        
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        writer->write(json, &file);
        
        file.close();
        
        std::cout << "Formatted JSON saved to: " << filename << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving formatted JSON: " << e.what() << std::endl;
        return false;
    }
}

void CALLBACK IVSInfoCallbackFunc(char* pIVSBuf, LONG nIVSType, LONG nIVSBufLen, LONG nFrameSeq, void* pReserved, void* pUserData){
    // pIVSBuf
    // [in] 智能数据。
    // nIVSType
    // [in] 数据类型。
    // nIVSBufLen
    // [in] 智能数据长度。
    // nFrameSeq
    // [in] 对应的视频帧序号。
    // pReserved
    // [in] 保留字段。
    // pUserData
    // [in] 用户数据。
    printf("pIVSBuf: %p\n", pIVSBuf);       // 打印指针地址
    printf("nIVSType: %ld\n", nIVSType);    // 打印 LONG 类型
    printf("nIVSBufLen: %ld\n", nIVSBufLen); // 打印缓冲区长度
    printf("nFrameSeq: %ld\n", nFrameSeq);   // 打印帧序号
    printf("pReserved: %p\n", pReserved);    // 打印保留指针
    // nIVSType==5,pIVSBuf为json数据
    if (nIVSType == 5 && pIVSBuf && nIVSBufLen > 0) {
        // 先检查数据是否有效
        printf("Raw buffer content (first 100 chars): %.100s\n", pIVSBuf);
        
        // 创建安全的字符串
        std::string jsonStr(pIVSBuf, nIVSBufLen);

        std::string filename = generateFileName(nFrameSeq);

        // 方法1: 使用修改后的函数
        Json::Value json = stringDataToJson(pIVSBuf, nIVSBufLen);
        std::cout << "Processed JSON:" << std::endl;
        std::cout << json.toStyledString() << std::endl;
    }else if(nIVSType == 7 && pIVSBuf && nIVSBufLen > 0){
        // IVSINFOTYPE_TRACK_EX_B0			= 7,   // intelligent structured data information
        cout<<"other type"<<endl;
    }

}


int main()
{
    InitTest();
    RunTest();
    EndTest();
    return 0;
}