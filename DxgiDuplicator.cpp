// DxgiDuplicator.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <dxgi.h>
#include <vector>
#include <d3d11.h>
#include <dxgi1_2.h>

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
    bool Init();
    bool GetFrame(void* destImage, UINT destSize, UINT* rowPitch);

    UINT GetImageHeight() {
        DXGI_OUTDUPL_DESC desc;
        outputDup->GetDesc(&desc);
        return desc.ModeDesc.Height;
    }

    UINT GetImageWidth() {
        DXGI_OUTDUPL_DESC desc;
        outputDup->GetDesc(&desc);
        return desc.ModeDesc.Height;
    }
private:
    ID3D11Device* pDevice;
    ID3D11DeviceContext* pDeviceContext;

    IDXGIDevice* dxgiDevice;
    IDXGIAdapter* dxgiAdapter;
    IDXGIOutput* dxgiOutput;
    IDXGIOutput1* dxgiOutput1;
    IDXGIOutputDuplication* outputDup;
    ID3D11Texture2D* texture2d;
};

bool DXGIDupMgr::Init()
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
        return -1;
    }

    // 2.创建DXGI设备
    hr = pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
    if (FAILED(hr)) {
        cout << "failed for get IDXGIDevice" << endl;
        return -1;
    }

    // 3.获取DXGI适配器
    hr = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&dxgiAdapter));
    if (FAILED(hr)) {
        cout << "failed for get IDXGIAdapter" << endl;
        return -1;
    }

    // 4.获取DXGI output
    hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);
    if (FAILED(hr)) {
        cout << "failed for get EnumOutputs" << endl;
        return -1;
    }

    // 5.获取DXGI output1    
    hr = dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&dxgiOutput1));
    if (FAILED(hr)) {
        cout << "failed for get IDXGIOutput1" << endl;
        return -1;
    }

    // 6.获取DXGI OutputDuplication    
    hr = dxgiOutput1->DuplicateOutput(pDevice, &outputDup);
    if (FAILED(hr)) {
        cout << "failed for DuplicateOutput" << endl;
        if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) {
            cout << "DXGI_ERROR_NOT_CURRENTLY_AVAILABLE" << endl;
        }
        return -1;
    }
    cout << "DuplicateOutput success" << endl;

    // 8.1 创建纹理    
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
        return -1;
    }

    cout << "CreateTexture2D success" << endl;

    return true;
}

bool DXGIDupMgr::GetFrame(void* destImage, UINT destSize, UINT* rowPitch)
{
    *rowPitch = 0;

    // 7. 获取桌面图像
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    IDXGIResource* idxgiRes;
    HRESULT hr = outputDup->AcquireNextFrame(500, &frameInfo, &idxgiRes);
    if (FAILED(hr)) {
        cout << "failed for AcquireNextFrame" << endl;
        return -1;
    }

    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        return true;
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
    pDeviceContext->CopyResource(texture2d, desktopTexture2d);

    // 【资源释放】9.2 拷贝完数据后，释放桌面纹理
    desktopTexture2d->Release();

    // 8.3 纹理映射(GPU -> CPU)
    D3D11_MAPPED_SUBRESOURCE resource;
    UINT subresource = D3D11CalcSubresource(0, 0, 0);
    pDeviceContext->Map(texture2d, subresource, D3D11_MAP_READ, 0, &resource);

    DXGI_OUTDUPL_DESC dxgiOutduplDesc;
    outputDup->GetDesc(&dxgiOutduplDesc);
    UINT height = dxgiOutduplDesc.ModeDesc.Height;
    *rowPitch = resource.RowPitch;

    cout << "height: " << height << " | rowPitch: " << *rowPitch << endl;

    UINT size = height * resource.RowPitch;
    memcpy_s(destImage, destSize, reinterpret_cast<BYTE*>(resource.pData), size);

    pDeviceContext->Unmap(texture2d, subresource);

    // 【资源释放】9.3 需要释放帧，对应AcquireNextFrame
    outputDup->ReleaseFrame();

    cout << "get frame success" << endl;
}

int main()
{
    DXGIDupMgr dxgiDupMgr;
    if (!dxgiDupMgr.Init()) {
        return 0;
    }
    
    BYTE* pBuf = new BYTE[10000000];
    for (int i = 0; i < 10; i++)
    {
        UINT rowPitch;
        cout << "get frame: " << i << endl;
        if (!dxgiDupMgr.GetFrame(pBuf, 10000000, &rowPitch)) {
            return 0;
        }

        char file_name[MAX_PATH] = {0};
        sprintf_s(file_name, "%d.bmp", i);
        saveBitmap(pBuf, rowPitch, dxgiDupMgr.GetImageHeight(), file_name);
    }
    delete pBuf;

    return 0;
}