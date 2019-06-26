#include <jni.h>
#include <string>
#include <opencv2/opencv.hpp>
#include <android/bitmap.h>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG,__VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)

#define DEFAULT_CARD_WIDTH 640
#define DEFAULT_CARD_HEIGHT 400
#define FIX_IDCARD_SIZE Size(DEFAULT_CARD_WIDTH, DEFAULT_CARD_HEIGHT)

using namespace cv;
using namespace std;

void bitmap2Mat(JNIEnv *env, Mat &mat, jobject bitmap);
void mat2Bitmap(JNIEnv *env, Mat &mat, jobject bitmap);

void bitmap2Mat(JNIEnv *env, Mat &mat, jobject bitmap) {
    // CV_8UC4 --> ARGB_8888
    // CV_8UC2 --> ARGB_565
    // 获取Bitmap信息
    AndroidBitmapInfo info;
    AndroidBitmap_getInfo(env, bitmap, &info);

    // 操作Bitmap需要先锁定画布
    void* pixels;
    AndroidBitmap_lockPixels(env, bitmap, &pixels);
    mat.create(info.height, info.width, CV_8UC4);

    if(info.format == ANDROID_BITMAP_FORMAT_RGBA_8888){
        // CV_8UC4
        Mat temp(info.height, info.width, CV_8UC4, pixels);
        temp.copyTo(mat);
    }else if(info.format == ANDROID_BITMAP_FORMAT_RGB_565){
        // CV_8UC2
        Mat temp(info.height, info.width, CV_8UC2, pixels);
        // 565-->rgbA
        cvtColor(temp, mat, COLOR_BGR5652BGRA);
    }
    // 其他不能自动适配

    // 解锁画布
    AndroidBitmap_unlockPixels(env, bitmap);
}

void mat2Bitmap(JNIEnv *env, Mat &mat, jobject bitmap) {
    AndroidBitmapInfo info;
    AndroidBitmap_getInfo(env, bitmap, &info);

    // 操作Bitmap需要先锁定画布
    void* pixels;
    AndroidBitmap_lockPixels(env, bitmap, &pixels);

    if(info.format == ANDROID_BITMAP_FORMAT_RGBA_8888){
        // CV_8UC4
        Mat temp(info.height, info.width, CV_8UC4, pixels);
        if(mat.type() == CV_8UC4){
            mat.copyTo(temp);
        }else if(mat.type() == CV_8UC2){
            // 565
            cvtColor(mat, temp, COLOR_BGR5652BGRA);
        }else if(mat.type() == CV_8UC1){
            // 灰度
            cvtColor(mat, temp, COLOR_GRAY2BGRA);
        }
    }else if(info.format == ANDROID_BITMAP_FORMAT_RGB_565){
        // CV_8UC2
        Mat temp(info.height, info.width, CV_8UC2, pixels);
        if(mat.type() == CV_8UC4){
            cvtColor(mat, temp, COLOR_BGRA2BGR565);
        }else if(mat.type() == CV_8UC2){
            // 565
            mat.copyTo(temp);
        }else if(mat.type() == CV_8UC1){
            // 灰度
            cvtColor(mat, temp, COLOR_GRAY2BGR565);
        }
    }
    // 其他不能自动适配

    // 解锁画布
    AndroidBitmap_unlockPixels(env, bitmap);
}

jobject createBitmap(JNIEnv* env, Mat srcData, jobject config){
    jclass java_bitmap_class = env->FindClass("android/graphics/Bitmap");
    jmethodID mid = env->GetStaticMethodID(java_bitmap_class, "createBitmap",
            "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jobject bitmap = env->CallStaticObjectMethod(java_bitmap_class, mid, srcData.cols, srcData.rows, config);
    mat2Bitmap(env, srcData, bitmap);
    return bitmap;
}

extern "C"
JNIEXPORT void JNICALL
Java_io_github_cnzcy_opencv_MainActivity_bitmapDistinguishFromJNI(JNIEnv *env, jobject instance,
                                                                  jobject bitmap) {

    Mat mat;
    bitmap2Mat(env, mat, bitmap);

    // 转灰度图  COLOR_BGRA2GRAY 注意格式的统一
    Mat gray_mat;
    cvtColor(mat, gray_mat, COLOR_BGRA2GRAY);

    // mat放回Bitmap
    mat2Bitmap(env, gray_mat, bitmap);

}

extern "C"
JNIEXPORT jobject JNICALL
Java_io_github_cnzcy_opencv_MainActivity_findIdNumber(JNIEnv *env, jobject instance, jobject bitmap,
                                                      jobject config) {
    Mat src_img;
    Mat dst_img;
    // 1. Bitmap -> Mat
    bitmap2Mat(env, src_img, bitmap);
    // 2. 归一化，统一分辨率 (这里实际没用到)
    Mat dst;
    resize(src_img, dst, FIX_IDCARD_SIZE);
    // 3. 转灰度图
    cvtColor(src_img, dst, COLOR_BGRA2GRAY);
    // 4. 二值化处理
    // 原 -> 处理后 阈值 颜色
    threshold(dst, dst, 100, 255, THRESH_BINARY);
    // 5. 膨胀
    // 膨胀的水平方向和垂直方向
    Mat erodeElement = getStructuringElement(MORPH_RECT, Size(40, 10));
    erode(dst, dst, erodeElement);
    // 6. 轮廓检测
    // Point 偏移量 0，0不偏移
    vector<vector<Point> > contours;
    vector<Rect> rects;
    findContours(dst, contours, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0, 0));
    for(int i=0; i<contours.size(); i++){
        // 7. 绘制轮廓区域
        Rect rect = boundingRect(contours.at(i));
        // 8. 遍历区域
        // 身份证宽高比 1:8 && 1:16
        if(rect.width > rect.height*8 && rect.width < rect.height*16){
            // 可能会找到连个区域分别时号码区和地址区
            rectangle(dst, rect, Scalar(0, 0, 255));
            rects.push_back(rect);
        }
    }
    // 9. 查找坐标最低的区域
    int lowPoint = 0;
    Rect finalRect;
    for(int i=0; i<rects.size(); i++){
        Rect rect = rects.at(i);
        Point point = rect.tl();// 左上角坐标
        if(point.y > lowPoint){
            lowPoint = point.y;
            finalRect = rect;
        }
    }
    // 10. 图像分割
    dst_img = src_img(finalRect);

    return createBitmap(env, dst_img, config);

}