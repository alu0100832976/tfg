#include "ros/ros.h"
#include "sensor_msgs/Image.h"
#include "image_transport/image_transport.h"
#include "image_transport/subscriber_filter.h"
#include "cv_bridge/cv_bridge.h"
#include "sensor_msgs/image_encodings.h"
#include "stereo_msgs/DisparityImage.h"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/calib3d/calib3d.hpp"

#include "message_filters/synchronizer.h"
#include "message_filters/sync_policies/approximate_time.h"
#include "message_filters/subscriber.h"

//Lo nuevo
#include "std_msgs/Float64.h"
#include "stdlib.h"

static const std::string OPENCV_WINDOWD = "Disparity";
//static const std::string OPENCV_WINDOWDC = "DisparityC";

typedef message_filters::sync_policies::ApproximateTime<
      sensor_msgs::Image, sensor_msgs::Image
      > MySyncPolicy;

class DisparityBM
{
private:
   ros::NodeHandle nh_;
   image_transport::ImageTransport it_;
   image_transport::SubscriberFilter image_left_sub_;
   image_transport::SubscriberFilter image_right_sub_;
   /*
   message_filters::Subscriber<sensor_msgs::CameraInfo> cameraInfoL_;
   message_filters::Subscriber<sensor_msgs::CameraInfo> cameraInfoR_;*/
   ros::Publisher disparity_bm_pub_;
   message_filters::Synchronizer<MySyncPolicy> sync_;

public:
   DisparityBM()
   : it_(nh_),
     image_left_sub_(it_, "/stereo/left/image_rect", 1),
     image_right_sub_(it_, "/stereo/right/image_rect", 1),
     /*cameraInfoR_(nh_, "/stereo/right/camera_info", 1),
     cameraInfoL_(nh_, "/stereo/left/camera_info", 1),*/
     sync_(MySyncPolicy(100),image_left_sub_, image_right_sub_)
   {
      sync_.registerCallback(boost::bind(&DisparityBM::disparity_callback, this, _1, _2));
      disparity_bm_pub_ = nh_.advertise<sensor_msgs::Image>("/disparity/bm", 1);    
      cv::namedWindow(OPENCV_WINDOWD);

      //cv::namedWindow(OPENCV_WINDOWDC);
   }

   ~DisparityBM()
   {
      cv::destroyWindow(OPENCV_WINDOWD);
      //cv::destroyWindow(OPENCV_WINDOWDC);
   }

   void disparity_callback(const sensor_msgs::ImageConstPtr& image_left_msg, const sensor_msgs::ImageConstPtr& image_right_msg)
   {
      ROS_INFO("Received stereo images");

      cv_bridge::CvImageConstPtr cv_ptr_left = cv_bridge::toCvCopy(image_left_msg, sensor_msgs::image_encodings::TYPE_8UC1);
      cv_bridge::CvImageConstPtr cv_ptr_right = cv_bridge::toCvCopy(image_right_msg, sensor_msgs::image_encodings::TYPE_8UC1);
      const cv::Mat left_image = cv_ptr_left->image;
      const cv::Mat right_image = cv_ptr_right->image;
      const cv::Mat imgDisparity16S = cv::Mat(left_image.rows, left_image.cols, CV_16S);
      const cv::Mat imgDisparity8U = cv::Mat(left_image.rows, left_image.cols, CV_8UC1);

      int ndisparity = 16*8;
      int SADWindowSize = 15;
      cv::Ptr<cv::StereoBM> sbm = cv::StereoBM::create(ndisparity, SADWindowSize);
      sbm->compute(left_image, right_image, imgDisparity16S);

      double minVal;
      double maxVal;

      cv::minMaxLoc(imgDisparity8U, &minVal, &maxVal);

      ROS_INFO("Min disp: %f Max value: %f", minVal, maxVal);

      imgDisparity16S.convertTo(imgDisparity8U, CV_8UC1, 255/(maxVal - minVal));
      cv::imshow(OPENCV_WINDOWD, imgDisparity8U);

      //Codigo para comparativas
      /*const cv::Mat imgDisparity16SC = cv::Mat(left_image.rows, left_image.cols, CV_16S);
      const cv::Mat imgDisparity8UC = cv::Mat(left_image.rows, left_image.cols, CV_8UC1);
      int ndisparityC = 16*8;
      int SADWindowSizeC = 21;
      cv::Ptr<cv::StereoBM> sbmC = cv::StereoBM::create(ndisparityC, SADWindowSizeC);
      sbmC->compute(left_image, right_image, imgDisparity16SC);
      double minValC;
      double maxValC;
      cv::minMaxLoc(imgDisparity8UC, &minValC, &maxValC);
      imgDisparity16SC.convertTo(imgDisparity8UC, CV_8UC1, 255/(maxValC - minValC));
      cv::imshow(OPENCV_WINDOWDC, imgDisparity8UC);
      */

      //Probando conversiones
      /*std_msgs::Float64 baseline;

      baseline = abs(infoL.P[3] - infoR.P[3]);
      ROS_INFO("baseline: %f", baseline);*/

      //Convirtiendo Mat en ros_msgs
      cv::Mat depthCvImage;         

      cv_bridge::CvImage out_msg;
      out_msg.header   = image_right_msg->header; // Same timestamp and tf frame as input image
      out_msg.encoding = sensor_msgs::image_encodings::TYPE_8UC1; // Or whatever
      out_msg.image    = imgDisparity8U; // Your cv::Mat

      sensor_msgs::ImagePtr depthImage;
      depthImage = out_msg.toImageMsg();

      //calcular depthImage

      float baseline = 33.109935;
      float focal = 0.794303;
      /*int size = depthImage.width * depthImage.height;
      sensor_msgs::Image currentDepthImage;
      currentDepthImage = *depthImage;
      for(int x=0; x<size; x++)
      {
         currentDepthImage.data[x]=(baseline*focal)/currentDepthImage.data[x];
      }*/

      cv::waitKey(3);

      disparity_bm_pub_.publish(depthImage);
   }
};

int main(int argc, char **argv)
{
   ros::init(argc, argv, "DisparityBM");
   DisparityBM dbm;
   ros::spin();
   return 0;
}