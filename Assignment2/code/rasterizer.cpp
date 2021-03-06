// clang-format off
//
// Created by goksu on 4/6/19.
//

#include <algorithm>
#include <vector>
#include "rasterizer.hpp"
#include <opencv2/opencv.hpp>
#include <math.h>


rst::pos_buf_id rst::rasterizer::load_positions(const std::vector<Eigen::Vector3f> &positions)
{
    auto id = get_next_id();
    pos_buf.emplace(id, positions);

    return {id};
}

rst::ind_buf_id rst::rasterizer::load_indices(const std::vector<Eigen::Vector3i> &indices)
{
    auto id = get_next_id();
    ind_buf.emplace(id, indices);

    return {id};
}

rst::col_buf_id rst::rasterizer::load_colors(const std::vector<Eigen::Vector3f> &cols)
{
    auto id = get_next_id();
    col_buf.emplace(id, cols);

    return {id};
}

auto to_vec4(const Eigen::Vector3f& v3, float w = 1.0f)
{
    return Vector4f(v3.x(), v3.y(), v3.z(), w);
}


static bool insideTriangle(float x, float y, const Vector3f* _v)
{   
    // 先说一下原理 : 视频里有提, 三个顶点分别跟P点做cross, 如果方向相同, 就在三角形内.  判断方向相同可以用Z, 也可以用dot(因为点乘同方向大于0, 反方向小于0)
    Vector3f p = {x, y, 0};   // 先定义一个点p, 其实这是一个二维的点, 但是我用二维向量做cross时报错, 所以就改成三维了,  这里需要注意
    // 下面是取三角形, 三个顶点的坐标a,b,c,  有一点需要注意的是 _v其实是一个Vector3f的数组, 我觉得这样分步写有助于理解
    Vector3f a = _v[0];
    Vector3f b = _v[1];
    Vector3f c = _v[2];
    // 三条边的向量
    Vector3f AB = b - a;
    Vector3f BC = c - b;
    Vector3f CA = a - c;
    // 三个顶点到P的向量
    Vector3f AP = p - a;
    Vector3f BP = p - b;
    Vector3f CP = p - c;
    // 分别做叉乘
    Vector3f d1 = AB.cross(AP);
    Vector3f d2 = BC.cross(BP);
    Vector3f d3 = CA.cross(CP);
    // 分别做点乘
    return (d1.dot(d2) > 0 && d2.dot(d3) > 0 && d3.dot(d1) > 0);
}

static std::tuple<float, float, float> computeBarycentric2D(float x, float y, const Vector3f* v)
{
    float c1 = (x*(v[1].y() - v[2].y()) + (v[2].x() - v[1].x())*y + v[1].x()*v[2].y() - v[2].x()*v[1].y()) / (v[0].x()*(v[1].y() - v[2].y()) + (v[2].x() - v[1].x())*v[0].y() + v[1].x()*v[2].y() - v[2].x()*v[1].y());
    float c2 = (x*(v[2].y() - v[0].y()) + (v[0].x() - v[2].x())*y + v[2].x()*v[0].y() - v[0].x()*v[2].y()) / (v[1].x()*(v[2].y() - v[0].y()) + (v[0].x() - v[2].x())*v[1].y() + v[2].x()*v[0].y() - v[0].x()*v[2].y());
    float c3 = (x*(v[0].y() - v[1].y()) + (v[1].x() - v[0].x())*y + v[0].x()*v[1].y() - v[1].x()*v[0].y()) / (v[2].x()*(v[0].y() - v[1].y()) + (v[1].x() - v[0].x())*v[2].y() + v[0].x()*v[1].y() - v[1].x()*v[0].y());
    return {c1,c2,c3};
}

void rst::rasterizer::draw(pos_buf_id pos_buffer, ind_buf_id ind_buffer, col_buf_id col_buffer, Primitive type)
{
    auto& buf = pos_buf[pos_buffer.pos_id];
    auto& ind = ind_buf[ind_buffer.ind_id];
    auto& col = col_buf[col_buffer.col_id];

    float f1 = (50 - 0.1) / 2.0;
    float f2 = (50 + 0.1) / 2.0;

    Eigen::Matrix4f mvp = projection * view * model;
    for (auto& i : ind)
    {
        Triangle t;
        Eigen::Vector4f v[] = {
                mvp * to_vec4(buf[i[0]], 1.0f),
                mvp * to_vec4(buf[i[1]], 1.0f),
                mvp * to_vec4(buf[i[2]], 1.0f)
        };
        //Homogeneous division
        for (auto& vec : v) {
            vec /= vec.w();
        }
        //Viewport transformation
        for (auto & vert : v)
        {
            vert.x() = 0.5*width*(vert.x()+1.0);
            vert.y() = 0.5*height*(vert.y()+1.0);
            vert.z() = vert.z() * f1 + f2;
        }

        for (int i = 0; i < 3; ++i)
        {
            t.setVertex(i, v[i].head<3>());
            t.setVertex(i, v[i].head<3>());
            t.setVertex(i, v[i].head<3>());
        }

        auto col_x = col[i[0]];
        auto col_y = col[i[1]];
        auto col_z = col[i[2]];

        t.setColor(0, col_x[0], col_x[1], col_x[2]);
        t.setColor(1, col_y[0], col_y[1], col_y[2]);
        t.setColor(2, col_z[0], col_z[1], col_z[2]);

        rasterize_triangle(t);
    }
}

//Screen space rasterization
void rst::rasterizer::rasterize_triangle(const Triangle& t) {
    auto v = t.toVector4();
    // 三角形3个顶点的齐次坐标
    Vector4f a = v[0];
    Vector4f b = v[1];
    Vector4f c = v[2];
    // 找到boudingbox的边界值
    int min_x = std::min(a.x(), std::min(b.x(), c.x()));
    int max_x = std::max(a.x(), std::max(b.x(), c.x()));
    int min_y = std::min(a.y(), std::min(b.y(), c.y()));
    int max_y = std::max(a.y(), std::max(b.y(), c.y()));
    // 一个1*1像素分成4个小像素
    std::vector<Vector2f> corners = {
        {0.25,0.25},{0.25,0.75},{0.75,0.25},{0.75,0.75}
    };

    for (int x = min_x; x <= max_x; x++)
    {
        for (int y = min_y; y <= max_y; y++)
        {
            float rate = 0;       // 4个像素在三角形内的采样率
            float minDepth = FLT_MAX;
            for (auto pos : corners)
            {
                if (insideTriangle(x + pos.x(), y + pos.y(), t.v)) 
                {
                    auto[alpha, beta, gamma] = computeBarycentric2D(x + pos.x(), y + pos.y(), t.v);
                    float w_reciprocal = 1.0 / (alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
                    float z_interpolated = alpha * v[0].z() / v[0].w() + beta * v[1].z() / v[1].w() + gamma * v[2].z() / v[2].w();
                    z_interpolated *= w_reciprocal;
                    minDepth = std::min(minDepth, z_interpolated);
                    rate += 0.25;  // 如果小像素在三角形内, 采样率就加0.25
                }
            }

            if (rate > 0 && minDepth < depth_buf[get_index(x, y)])
            {
                depth_buf[get_index(x, y)] = minDepth;   // 这一步也挺重要的,  将深度测试结果更新到深度缓冲区里, 如果不这样, 会出现蓝色三角形在绿色的上面
                Eigen::Vector3f cur_piexl = {x, y, minDepth};
                set_pixel(cur_piexl, t.getColor() * rate);  // 将颜色值乘以, 最后计算出的采样率
            }
        }
    }
}

void rst::rasterizer::set_model(const Eigen::Matrix4f& m)
{
    model = m;
}

void rst::rasterizer::set_view(const Eigen::Matrix4f& v)
{
    view = v;
}

void rst::rasterizer::set_projection(const Eigen::Matrix4f& p)
{
    projection = p;
}

void rst::rasterizer::clear(rst::Buffers buff)
{
    if ((buff & rst::Buffers::Color) == rst::Buffers::Color)
    {
        std::fill(frame_buf.begin(), frame_buf.end(), Eigen::Vector3f{0, 0, 0});
    }
    if ((buff & rst::Buffers::Depth) == rst::Buffers::Depth)
    {
        std::fill(depth_buf.begin(), depth_buf.end(), std::numeric_limits<float>::infinity());
    }
}

rst::rasterizer::rasterizer(int w, int h) : width(w), height(h)
{
    frame_buf.resize(w * h);
    depth_buf.resize(w * h);
}

int rst::rasterizer::get_index(int x, int y)
{
    return (height-1-y)*width + x;
}

void rst::rasterizer::set_pixel(const Eigen::Vector3f& point, const Eigen::Vector3f& color)
{
    //old index: auto ind = point.y() + point.x() * width;
    auto ind = (height-1-point.y())*width + point.x();
    frame_buf[ind] = color;

}

// clang-format on