// DxgiDuplicator.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <dxgi.h>
#include <vector>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <vector>

#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "D3D11.lib")

using namespace std;

#define IID_PPV_ARGS(ppType) __uuidof(**(ppType)), IID_PPV_ARGS_Helper(ppType)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)

const static D3D_FEATURE_LEVEL featureLevels[] = {
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
};

void saveBitmap(unsigned char* bitmap_data, int rowPitch, int height, const char* filename)
{
    // A file is created, this is where we will save the screen capture.

    FILE* f;

    BITMAPFILEHEADER   bmfHeader;
    BITMAPINFOHEADER   bi;

    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = rowPitch / 4;
    //Make the size negative if the image is upside down.
    bi.biHeight = -height;
    //There is only one plane in RGB color space where as 3 planes in YUV.
    bi.biPlanes = 1;
    //In windows RGB, 8 bit - depth for each of R, G, B and alpha.
    bi.biBitCount = 32;
    //We are not compressing the image.
    bi.biCompression = BI_RGB;
    // The size, in bytes, of the image. This may be set to zero for BI_RGB bitmaps.
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    // rowPitch = the size of the row in bytes.
    DWORD dwSizeofImage = rowPitch * height;

    // Add the size of the headers to the size of the bitmap to get the total file size
    DWORD dwSizeofDIB = dwSizeofImage + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    //Offset to where the actual bitmap bits start.
    bmfHeader.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER);

    //Size of the file
    bmfHeader.bfSize = dwSizeofDIB;

    //bfType must always be BM for Bitmaps
    bmfHeader.bfType = 0x4D42; //BM   

                               // TODO: Handle getting current directory
    fopen_s(&f, filename, "wb");

    DWORD dwBytesWritten = 0;
    dwBytesWritten += fwrite(&bmfHeader, sizeof(BITMAPFILEHEADER), 1, f);
    dwBytesWritten += fwrite(&bi, sizeof(BITMAPINFOHEADER), 1, f);
    dwBytesWritten += fwrite(bitmap_data, 1, dwSizeofImage, f);

    fclose(f);
}

class DXGIDupMgr {
public:
    DXGIDupMgr();
    ~DXGIDupMgr();
    bool InitDevice();
    bool InitOutput(int monitorIdx);
    bool Init();
    int GetFrame(int idx, void* destImage, UINT destSize, UINT* rowPitch);

    UINT GetImageHeight(int idx) {
        DXGI_OUTDUPL_DESC desc;
        outputDupV[idx]->GetDesc(&desc);
        return desc.ModeDesc.Height;
    }

    UINT GetImageWidth(int idx) {
        DXGI_OUTDUPL_DESC desc;
        outputDupV[idx]->GetDesc(&desc);
        return desc.ModeDesc.Height;
    }
private:
    ID3D11Device* pDevice;
    ID3D11DeviceContext* pDeviceContext;

    IDXGIDevice* dxgiDevice;
    IDXGIAdapter* dxgiAdapter;

    vector<IDXGIOutput*> dxgiOutputV;
    vector<IDXGIOutput1*> dxgiOutput1V;
    vector<IDXGIOutputDuplication*> outputDupV;
    vector<ID3D11Texture2D*> texture2dV;
};

DXGIDupMgr::DXGIDupMgr() : 
    pDevice(nullptr), pDeviceContext(nullptr), 
    dxgiDevice(nullptr), dxgiAdapter(nullptr)
{}

DXGIDupMgr::~DXGIDupMgr()
{
    for (auto e : texture2dV) {
        e->Release();
    }
    for (auto e : outputDupV) {
        e->Release();
    }
    for (auto e : dxgiOutput1V) {
        e->Release();
    }
    for (auto e : dxgiOutputV) {
        e->Release();
    }
    if (dxgiAdapter)
        dxgiAdapter->Release();
    if (dxgiDevice)
        dxgiDevice->Release();
    if (pDevice)
        pDevice->Release();
    if (pDeviceContext)
        pDeviceContext->Release();
}

bool DXGIDupMgr::Init()
{
    bool ret = InitDevice();
    ret = InitOutput(0);
    ret = InitOutput(1);

    return ret;
}

bool DXGIDupMgr::InitDevice()
{
    // 1.创建D3D设备
    D3D_FEATURE_LEVEL levelUsed = D3D_FEATURE_LEVEL_10_0;

    uint32_t createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createFlags,
        featureLevels, sizeof(featureLevels) / sizeof(D3D_FEATURE_LEVEL),
        D3D11_SDK_VERSION, &pDevice, &levelUsed, &pDeviceContext);
    if (FAILED(hr)) {
        cout << "failed for D3D11CreateDevice" << endl;
        return false;
    }

    // 2.创建DXGI设备
    hr = pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
    if (FAILED(hr)) {
        cout << "failed for get IDXGIDevice" << endl;
        return false;
    }

    // 3.获取DXGI适配器
    hr = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&dxgiAdapter));
    if (FAILED(hr)) {
        cout << "failed for get IDXGIAdapter" << endl;
        return false;
    }

    // 3.1 枚举所有显示器
    for (int i = 0; i < 6; i ++) {
        IDXGIOutput* dxgiOutput;
        HRESULT hr = dxgiAdapter->EnumOutputs(i, &dxgiOutput);
        if (FAILED(hr)) {
            cout << "failed for get EnumOutputs" << endl;
            break;
        }
        DXGI_OUTPUT_DESC desc;
        dxgiOutput->GetDesc(&desc);
        cout << "============ monitor " << i << endl;
        cout << desc.DeviceName << endl;
        cout << desc.Rotation << endl;
        cout << desc.DesktopCoordinates.left << endl;
        cout << desc.DesktopCoordinates.top << endl;
        cout << desc.DesktopCoordinates.right << endl;
        cout << desc.DesktopCoordinates.bottom << endl;
    }

    return true;
}

bool DXGIDupMgr::InitOutput(int monitorIdx)
{
    // 4.获取DXGI output
    IDXGIOutput* dxgiOutput;
    HRESULT hr = dxgiAdapter->EnumOutputs(monitorIdx, &dxgiOutput);
    if (FAILED(hr)) {
        cout << "failed for get EnumOutputs" << endl;
        return false;
    }     
    dxgiOutputV.emplace_back(dxgiOutput);

    // 5.获取DXGI output1  
    IDXGIOutput1* dxgiOutput1;
    hr = dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&dxgiOutput1));
    if (FAILED(hr)) {
        cout << "failed for get IDXGIOutput1" << endl;
        return false;
    }
    dxgiOutput1V.emplace_back(dxgiOutput1);

    // 6.获取DXGI OutputDuplication    
    IDXGIOutputDuplication* outputDup;
    hr = dxgiOutput1->DuplicateOutput(pDevice, &outputDup);
    if (FAILED(hr)) {
        cout << "failed for DuplicateOutput" << endl;
        if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) {
            cout << "DXGI_ERROR_NOT_CURRENTLY_AVAILABLE" << endl;
        }
        return false;
    }
    outputDupV.emplace_back(outputDup);
    cout << "DuplicateOutput success" << endl;

    // 8.1 创建纹理
    ID3D11Texture2D* texture2d;
    D3D11_TEXTURE2D_DESC texture2dDesc;

    DXGI_OUTDUPL_DESC dxgiOutduplDesc;
    outputDup->GetDesc(&dxgiOutduplDesc);
    cout << dxgiOutduplDesc.ModeDesc.Width << endl;
    cout << dxgiOutduplDesc.ModeDesc.Height << endl;
    texture2dDesc.Width = dxgiOutduplDesc.ModeDesc.Width;
    texture2dDesc.Height = dxgiOutduplDesc.ModeDesc.Height;
    texture2dDesc.Format = dxgiOutduplDesc.ModeDesc.Format;
    texture2dDesc.ArraySize = 1;
    texture2dDesc.BindFlags = 0;
    texture2dDesc.MiscFlags = 0;
    texture2dDesc.SampleDesc.Count = 1;
    texture2dDesc.SampleDesc.Quality = 0;
    texture2dDesc.MipLevels = 1;
    texture2dDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    texture2dDesc.Usage = D3D11_USAGE_STAGING;

    hr = pDevice->CreateTexture2D(&texture2dDesc, NULL, &texture2d);
    if (FAILED(hr)) {
        cout << "failed for CreateTexture2D" << endl;
        return false;
    }
    texture2dV.emplace_back(texture2d);
    cout << "CreateTexture2D success" << endl;

    return true;
}

int DXGIDupMgr::GetFrame(int idx, void* destImage, UINT destSize, UINT* rowPitch)
{
    cout << "get frame start" << endl;
    *rowPitch = 0;

    // 7. 获取桌面图像
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    IDXGIResource* idxgiRes;
    HRESULT hr = outputDupV[idx]->AcquireNextFrame(500, &frameInfo, &idxgiRes);
    if (FAILED(hr)) {
        printf("failed for AcquireNextFrame: %x\n", hr);
        if (hr == DXGI_ERROR_ACCESS_LOST)
            cout << "DXGI_ERROR_ACCESS_LOST" << endl;
        if (hr == DXGI_ERROR_INVALID_CALL)
            cout << "DXGI_ERROR_INVALID_CALL " << endl;
        if (hr == E_INVALIDARG)
            cout << "E_INVALIDARG  " << endl;
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            cout << "DXGI_ERROR_WAIT_TIMEOUT  " << endl;
            return 1;
        }
        return -1;
    }

    ID3D11Texture2D* desktopTexture2d;
    hr = idxgiRes->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&desktopTexture2d));
    if (FAILED(hr)) {
        cout << "failed for get desktopTexture2d" << endl;
        return -1;
    }

    // 【资源释放】9.1 查询到数据后，释放IDXG资源
    idxgiRes->Release();
    // 8. 将桌面图像拷贝出来
    // 8.2 复制纹理(GPU间复制)
    pDeviceContext->CopyResource(texture2dV[idx], desktopTexture2d);

    // 【资源释放】9.2 拷贝完数据后，释放桌面纹理
    desktopTexture2d->Release();

    // 8.3 纹理映射(GPU -> CPU)
    D3D11_MAPPED_SUBRESOURCE resource;
    UINT subresource = D3D11CalcSubresource(0, 0, 0);
    pDeviceContext->Map(texture2dV[idx], subresource, D3D11_MAP_READ, 0, &resource);

    DXGI_OUTDUPL_DESC dxgiOutduplDesc;
    outputDupV[idx]->GetDesc(&dxgiOutduplDesc);
    UINT height = dxgiOutduplDesc.ModeDesc.Height;
    *rowPitch = resource.RowPitch;

    cout << "height: " << height << " | rowPitch: " << *rowPitch << endl;

    UINT size = height * resource.RowPitch;
    memcpy_s(destImage, destSize, reinterpret_cast<BYTE*>(resource.pData), size);

    pDeviceContext->Unmap(texture2dV[idx], subresource);

    // 【资源释放】9.3 需要释放帧，对应AcquireNextFrame
    outputDupV[idx]->ReleaseFrame();

    cout << "get frame success" << endl;

    return 0;
}

int main()
{
    DXGIDupMgr dxgiDupMgr;
    if (!dxgiDupMgr.Init()) {
        return 0;
    }
    
    BYTE* pBuf = new BYTE[10000000];
    int index = 0;
    for (int i = 0; i < 10;)
    {
        int idx = index % 2;
        UINT rowPitch;
        cout << "get frame: " << i <<" ["<<idx<<"]" << endl;
        int ret = dxgiDupMgr.GetFrame(idx, pBuf, 10000000, &rowPitch);
        if (ret < 0) {
            return 0;
        } else if (ret > 0) {
            continue;
        }

        char file_name[MAX_PATH] = {0};
        sprintf_s(file_name, "%d.bmp", i);
        saveBitmap(pBuf, rowPitch, dxgiDupMgr.GetImageHeight(idx), file_name);
        index++;
        i++;
    }
    delete pBuf;

    return 0;
}