#ifndef RASTERIZER_TEXTURE_H
#define RASTERIZER_TEXTURE_H
#include "global.hpp"
#include <eigen3/Eigen/Eigen>
#include <opencv2/opencv.hpp>
class Texture{
private:
    cv::Mat image_data;

public:
    Texture(const std::string& name)
    {
        image_data = cv::imread(name);
        cv::cvtColor(image_data, image_data, cv::COLOR_RGB2BGR);
        width = image_data.cols;
        height = image_data.rows;
    }

    int width, height;

    Eigen::Vector3f getColor(float u, float v)
    {
        auto u_img = u * width;
        auto v_img = (1 - v) * height;
        auto color = image_data.at<cv::Vec3b>(v_img, u_img);
        return Eigen::Vector3f(color[0], color[1], color[2]);
    }

    Eigen::Vector3f getColorBilinear(float u, float v)
    {
        u = u < 0 ? 0 : u;
        u = u > 1 ? 1 : u;
        v = v < 0 ? 0 : v;
        v = v > 1 ? 1 : v;

        float u_img = u * width;
        float v_img = (1 - v) * height;

        // 先取邻近4个点
        float u_min = std::floor(u_img);
        float u_max = std::min((float)width, std::ceil(u_img));
        float v_min = std::floor(v_img);
        float v_max = std::min((float)height, std::ceil(v_img));

        cv::Vec3b p1 = image_data.at<cv::Vec3b>(v_max, u_min);
        cv::Vec3b p2 = image_data.at<cv::Vec3b>(v_max, u_max);
        cv::Vec3b p3 = image_data.at<cv::Vec3b>(v_min, u_min);
        cv::Vec3b p4 = image_data.at<cv::Vec3b>(v_min, u_max);

        float u_rate = (u_img - u_min) / (u_max - u_min);
        float v_rate = (v_img - v_max) / (v_min - v_max);

        cv::Vec3b top = (1 - u_rate) * p3 + u_rate * p4;
        cv::Vec3b bottom = (1 - u_rate) * p1 + u_rate * p2;
        cv::Vec3b color = (1 - v_rate) * bottom + v_rate * top;
        return Eigen::Vector3f(color[0], color[1], color[2]);
    }
};
#endif //RASTERIZER_TEXTURE_H
