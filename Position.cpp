#include "Position.h"

Position::Position(int x, int y, int width, int height) :
    x_(x),
    y_(y),
    width_(width),
    height_(height)
{}

int Position::left() const {
    return x_;
}

int Position::right() const {
    return x_ + width_;
}

int Position::top() const {
    return y_;
}

int Position::bottom() const {
    return y_ + height_;
}

int Position::centerX() const {
    return x_ + width_ / 2;
}

int Position::centerY() const {
    return y_ + height_ / 2;
}

cv::Point Position::topLeft() const {
    return cv::Point(x_, y_);
}

cv::Point Position::topRight() const {
    return cv::Point(x_ + width_, y_);
}

cv::Point Position::bottomLeft() const {
    return cv::Point(x_, y_ + height_);
}

cv::Point Position::bottomRight() const {
    return cv::Point(x_ + width_, y_ + height_);
}

int Position::halfWidth() const {
    return width_ / 2;
}

int Position::thirdWidth() const {
    return width_ / 3;
}

int Position::quarterWidth() const {
    return width_ / 4;
}

int Position::sixthWidth() const {
    return width_ / 6;
}
    
int Position::eighthWidth() const {
    return width_ / 8;
}

int Position::halfHeight() const {
    return height_ / 2;
}

int Position::thirdHeight() const {
    return height_ / 3;
}

int Position::quarterHeight() const {
    return height_ / 4;
}

int Position::sixthHeight() const {
    return height_ / 6;
}
    
int Position::eighthHeight() const {
    return height_ / 8;
}

int Position::x() const {
    return x_;
}

int Position::y() const {
    return y_;
}

int Position::width() const {
    return width_;
}

int Position::height() const {
    return height_;
}

void Position::setX(int x) {
    x_ = x;
}

void Position::setY(int y) {
    y_ = y;
}

void Position::setWidth(int width) {
    width_ = width;
}

void Position::setHeight(int height) {
    height_ = height;
}

bool Position::contains(int x, int y) const {
    return x >= left() && x < right() && y >= top() && y < bottom();
}

bool Position::contains(const cv::Point& point) const {
    return contains(point.x, point.y);
}

bool Position::intersects(const Position& other) const {
    return right() > other.left() && left() < other.right() &&
           bottom() > other.top() && top() < other.bottom();
}