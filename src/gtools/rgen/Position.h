#pragma once

#include "pch.h"

/**
 * The Position class allows for us create relative positioning commands 
 * to layout sections and text on the image frame
 * 
 * The code will make some commonly use positions to return based on the width and height of a given box
 * and the x and y values it is positioned at 
 */

class Position {
public:
    Position(int x, int y, int width, int height);

    int left() const;
    int right() const;
    int top() const;
    int bottom() const;
    int centerX() const;
    int centerY() const;

    cv::Point topLeft() const;
    cv::Point topRight() const;
    cv::Point bottomLeft() const;
    cv::Point bottomRight() const;

    int halfWidth() const;
    int thirdWidth() const;
    int quarterWidth() const;
    int sixthWidth() const;
    int eighthWidth() const;

    int halfHeight() const;
    int thirdHeight() const;
    int quarterHeight() const;
    int sixthHeight() const;
    int eighthHeight() const;

    int x() const;
    int y() const;
    int width() const;
    int height() const;

    void setX(int x);
    void setY(int y);
    void setWidth(int width);
    void setHeight(int height);

    bool contains(int x, int y) const;
    bool contains(const cv::Point& point) const;
    bool intersects(const Position& other) const;

private:
    int x_;
    int y_;
    int width_;
    int height_;
};