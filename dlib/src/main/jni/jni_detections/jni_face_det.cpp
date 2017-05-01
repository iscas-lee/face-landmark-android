/*
 * jni_pedestrian_det.cpp using google-style
 *
 *  Created on: Oct 20, 2015
 *      Author: Tzutalin
 *
 *  Copyright (c) 2015 Tzutalin. All rights reserved.
 */
#include <android/bitmap.h>
#include <jni_common/jni_bitmap2mat.h>
#include <jni_common/jni_primitives.h>
#include <jni_common/jni_fileutils.h>
#include <jni_common/jni_utils.h>
#include <detector.h>
#include <jni.h>


using namespace cv;

extern JNI_VisionDetRet* g_pJNI_VisionDetRet;

namespace {

#define JAVA_NULL 0
using DetectorPtr = DLibHOGFaceDetector*;

class JNI_FaceDet {
 public:
  JNI_FaceDet(JNIEnv* env) {
    jclass clazz = env->FindClass(CLASSNAME_FACE_DET);
    mNativeContext = env->GetFieldID(clazz, "mNativeFaceDetContext", "J");
    env->DeleteLocalRef(clazz);
  }

  DetectorPtr getDetectorPtrFromJava(JNIEnv* env, jobject thiz) {
    DetectorPtr const p = (DetectorPtr)env->GetLongField(thiz, mNativeContext);
    return p;
  }

  void setDetectorPtrToJava(JNIEnv* env, jobject thiz, jlong ptr) {
    env->SetLongField(thiz, mNativeContext, ptr);
  }

  jfieldID mNativeContext;
};

// Protect getting/setting and creating/deleting pointer between java/native
std::mutex gLock;

std::shared_ptr<JNI_FaceDet> getJNI_FaceDet(JNIEnv* env) {
  static std::once_flag sOnceInitflag;
  static std::shared_ptr<JNI_FaceDet> sJNI_FaceDet;
  std::call_once(sOnceInitflag, [env]() {
    sJNI_FaceDet = std::make_shared<JNI_FaceDet>(env);
  });
  return sJNI_FaceDet;
}

DetectorPtr const getDetectorPtr(JNIEnv* env, jobject thiz) {
  std::lock_guard<std::mutex> lock(gLock);
  return getJNI_FaceDet(env)->getDetectorPtrFromJava(env, thiz);
}

// The function to set a pointer to java and delete it if newPtr is empty
void setDetectorPtr(JNIEnv* env, jobject thiz, DetectorPtr newPtr) {
  std::lock_guard<std::mutex> lock(gLock);
  DetectorPtr oldPtr = getJNI_FaceDet(env)->getDetectorPtrFromJava(env, thiz);
  if (oldPtr != JAVA_NULL) {
    DLOG(INFO) << "setMapManager delete old ptr : " << oldPtr;
    delete oldPtr;
  }

  if (newPtr != JAVA_NULL) {
    DLOG(INFO) << "setMapManager set new ptr : " << newPtr;
  }

  getJNI_FaceDet(env)->setDetectorPtrToJava(env, thiz, (jlong)newPtr);
}

}  // end unnamespace

#ifdef __cplusplus
extern "C" {
#endif


#define DLIB_FACE_JNI_METHOD(METHOD_NAME) \
  Java_com_tzutalin_dlib_FaceDet_##METHOD_NAME

// add by simon at 2017/05/01 -- start
std::vector<cv::Point3d> get_3d_model_points()
{
  std::vector<cv::Point3d> modelPoints;

  modelPoints.push_back(cv::Point3d(0.0f, 0.0f, 0.0f)); //The first must be (0,0,0) while using POSIT
  modelPoints.push_back(cv::Point3d(0.0f, -330.0f, -65.0f));
  modelPoints.push_back(cv::Point3d(-225.0f, 170.0f, -135.0f));
  modelPoints.push_back(cv::Point3d(225.0f, 170.0f, -135.0f));
  modelPoints.push_back(cv::Point3d(-150.0f, -150.0f, -125.0f));
  modelPoints.push_back(cv::Point3d(150.0f, -150.0f, -125.0f));

  return modelPoints;

}

std::vector<cv::Point2d> get_2d_image_points(dlib::full_object_detection &d)
{
  std::vector<cv::Point2d> image_points;
  image_points.push_back( cv::Point2d( d.part(30).x(), d.part(30).y() ) );    // Nose tip
  image_points.push_back( cv::Point2d( d.part(8).x(), d.part(8).y() ) );      // Chin
  image_points.push_back( cv::Point2d( d.part(36).x(), d.part(36).y() ) );    // Left eye left corner
  image_points.push_back( cv::Point2d( d.part(45).x(), d.part(45).y() ) );    // Right eye right corner
  image_points.push_back( cv::Point2d( d.part(48).x(), d.part(48).y() ) );    // Left Mouth corner
  image_points.push_back( cv::Point2d( d.part(54).x(), d.part(54).y() ) );    // Right mouth corner
  return image_points;

}

cv::Mat get_camera_matrix(float focal_length, cv::Point2d center)
{
  cv::Mat camera_matrix = (cv::Mat_<double>(3,3) << focal_length, 0, center.x, 0 , focal_length, center.y, 0, 0, 1);
  return camera_matrix;
}
// add by simon at 2017/05/01 -- end

void JNIEXPORT
    DLIB_FACE_JNI_METHOD(jniNativeClassInit)(JNIEnv* env, jclass _this) {}

jobjectArray getDetectResult(JNIEnv* env, DetectorPtr faceDetector,
                             const int& size) {
  LOG(INFO) << "getFaceRet";
  jobjectArray jDetRetArray = JNI_VisionDetRet::createJObjectArray(env, size);
  for (int i = 0; i < size; i++) {
    jobject jDetRet = JNI_VisionDetRet::createJObject(env);
    env->SetObjectArrayElement(jDetRetArray, i, jDetRet);
    dlib::rectangle rect = faceDetector->getResult()[i];
    g_pJNI_VisionDetRet->setRect(env, jDetRet, rect.left(), rect.top(),
                                 rect.right(), rect.bottom());
    g_pJNI_VisionDetRet->setLabel(env, jDetRet, "face");
    std::unordered_map<int, dlib::full_object_detection>& faceShapeMap =
        faceDetector->getFaceShapeMap();
    if (faceShapeMap.find(i) != faceShapeMap.end()) {
      dlib::full_object_detection shape = faceShapeMap[i];
      for (unsigned long j = 0; j < shape.num_parts(); j++) {
        int x = shape.part(j).x();
        int y = shape.part(j).y();
        // Call addLandmark
        g_pJNI_VisionDetRet->addLandmark(env, jDetRet, x, y);
      }
    }
  }
  return jDetRetArray;
}

jobjectArray getDetectResult2(JNIEnv* env, DetectorPtr faceDetector, const int& size, cv::Mat &img) {
  LOG(INFO) << "getFaceRet";
  jobjectArray jDetRetArray = JNI_VisionDetRet::createJObjectArray(env, size);
  for (int i = 0; i < size; i++) {
      jobject jDetRet = JNI_VisionDetRet::createJObject(env);
      env->SetObjectArrayElement(jDetRetArray, i, jDetRet);
      dlib::rectangle rect = faceDetector->getResult()[i];
      g_pJNI_VisionDetRet->setRect(env, jDetRet, rect.left(), rect.top(),
              rect.right(), rect.bottom());
      g_pJNI_VisionDetRet->setLabel(env, jDetRet, "face");
      std::unordered_map<int, dlib::full_object_detection>& faceShapeMap =
              faceDetector->getFaceShapeMap();
      if (faceShapeMap.find(i) != faceShapeMap.end()) {
          dlib::full_object_detection shape = faceShapeMap[i];
          for (unsigned long j = 0; j < shape.num_parts(); j++) {
            int x = shape.part(j).x();
            int y = shape.part(j).y();
            // Call addLandmark
            g_pJNI_VisionDetRet->addLandmark(env, jDetRet, x, y);
          }

          // add by simon at 2017/05/01 -- start
          // 推算Head Pose
          std::vector<cv::Point3d> model_points = get_3d_model_points();
          std::vector<cv::Point2d> image_points = get_2d_image_points(shape);
          double focal_length = img.cols;
          cv::Mat camera_matrix = get_camera_matrix(focal_length, cv::Point2d(img.cols/2,img.rows/2));
          cv::Mat rotation_vector;
          cv::Mat rotation_matrix;
          cv::Mat translation_vector;

          cv::Mat dist_coeffs = cv::Mat::zeros(4,1,cv::DataType<double>::type);
          cv::solvePnP(model_points, image_points, camera_matrix, dist_coeffs, rotation_vector, translation_vector);

          std::vector<cv::Point3d> nose_end_point3D;
          std::vector<cv::Point2d> nose_end_point2D;
          nose_end_point3D.push_back(cv::Point3d(0, 0, 400.0));
          nose_end_point3D.push_back(cv::Point3d(0, 400.0, 0));
          nose_end_point3D.push_back(cv::Point3d(400.0, 0, 0));

          cv::projectPoints(nose_end_point3D, rotation_vector, translation_vector, camera_matrix, dist_coeffs, nose_end_point2D);

          g_pJNI_VisionDetRet->addPosePoint(env, jDetRet, nose_end_point2D[0].x, nose_end_point2D[0].y);
          g_pJNI_VisionDetRet->addPosePoint(env, jDetRet, nose_end_point2D[1].x, nose_end_point2D[1].y);
          g_pJNI_VisionDetRet->addPosePoint(env, jDetRet, nose_end_point2D[2].x, nose_end_point2D[2].y);
          // add by simon at 2017/05/01 -- end
      }
  }
  return jDetRetArray;
}

JNIEXPORT jobjectArray JNICALL
    DLIB_FACE_JNI_METHOD(jniDetect)(JNIEnv* env, jobject thiz,
                                    jstring imgPath) {
  LOG(INFO) << "jniFaceDet";
  const char* img_path = env->GetStringUTFChars(imgPath, 0);
  DetectorPtr detPtr = getDetectorPtr(env, thiz);
  int size = detPtr->det(std::string(img_path));
  env->ReleaseStringUTFChars(imgPath, img_path);
  LOG(INFO) << "det face size: " << size;
  return getDetectResult(env, detPtr, size);
}

JNIEXPORT jobjectArray JNICALL
    DLIB_FACE_JNI_METHOD(jniBitmapDetect)(JNIEnv* env, jobject thiz,
                                          jobject bitmap) {
  LOG(INFO) << "jniBitmapFaceDet";
  cv::Mat rgbaMat;
  cv::Mat bgrMat;
  jniutils::ConvertBitmapToRGBAMat(env, bitmap, rgbaMat, true);
  cv::cvtColor(rgbaMat, bgrMat, cv::COLOR_RGBA2BGR);
  DetectorPtr detPtr = getDetectorPtr(env, thiz);
  jint size = detPtr->det(bgrMat);
#if 0
  cv::Mat rgbMat;
  cv::cvtColor(bgrMat, rgbMat, cv::COLOR_BGR2RGB);
  cv::imwrite("/sdcard/ret.jpg", rgbaMat);
#endif
  LOG(INFO) << "det face size: " << size;
  //return getDetectResult(env, detPtr, size);
  // modify by simon at 2017/05/01
  return getDetectResult2(env, detPtr, size, bgrMat);
}

jint JNIEXPORT JNICALL DLIB_FACE_JNI_METHOD(jniInit)(JNIEnv* env, jobject thiz,
                                                     jstring jLandmarkPath) {
  LOG(INFO) << "jniInit";
  std::string landmarkPath = jniutils::convertJStrToString(env, jLandmarkPath);
  DetectorPtr detPtr = new DLibHOGFaceDetector(landmarkPath);
  setDetectorPtr(env, thiz, detPtr);
  ;
  return JNI_OK;
}

jint JNIEXPORT JNICALL
    DLIB_FACE_JNI_METHOD(jniDeInit)(JNIEnv* env, jobject thiz) {
  LOG(INFO) << "jniDeInit";
  setDetectorPtr(env, thiz, JAVA_NULL);
  return JNI_OK;
}

#ifdef __cplusplus
}
#endif
