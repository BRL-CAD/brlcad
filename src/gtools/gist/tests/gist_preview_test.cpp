/*              G I S T _ P R E V I E W _ T E S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "pch.h"

namespace {

bool
sameImage(const cv::Mat &left, const cv::Mat &right)
{
    if (left.size() != right.size() || left.type() != right.type()) {
	return false;
    }

    cv::Mat difference;
    cv::absdiff(left, right, difference);
    return cv::countNonZero(difference.reshape(1)) == 0;
}

} // namespace

int
main(int argc, const char **argv)
{
    if (argc != 2) {
	std::cerr << "Usage: " << argv[0] << " output-directory\n";
	return 1;
    }

    Options options;
    if (options.getWidth() != Options::CANONICAL_REPORT_WIDTH ||
	options.getLength() != Options::CANONICAL_REPORT_LENGTH) {
	std::cerr << "Default report dimensions do not match the layout canvas\n";
	return 1;
    }

    constexpr int previewPpi = 72;
    options.setPPI(previewPpi);
    const int expectedWidth = static_cast<int>(std::lround(
	static_cast<double>(Options::CANONICAL_REPORT_WIDTH) * previewPpi /
	Options::DEFAULT_REPORT_PPI));
    const int expectedHeight = static_cast<int>(std::lround(
	static_cast<double>(Options::CANONICAL_REPORT_LENGTH) * previewPpi /
	Options::DEFAULT_REPORT_PPI));
    if (options.getWidth() != expectedWidth || options.getLength() != expectedHeight) {
	std::cerr << "Preview dimensions do not preserve the canonical aspect ratio\n";
	return 1;
    }

    const std::filesystem::path outputDirectory(argv[1]);
    const std::filesystem::path fullPath = outputDirectory / "gist_preview_test_full.bmp";
    const std::filesystem::path previewPath = outputDirectory / "gist_preview_test_low.bmp";

    constexpr int pageMargin = 23;
    constexpr int bannerBottom = 122;
    constexpr int factsLeft = 2900;
    constexpr int factsTop = 131;
    constexpr int factsBottom = 1605;
    constexpr int dividerLeft = 80;
    constexpr int dividerRight = 2800;
    constexpr int dividerY = 1800;
    constexpr int dividerWidth = 5;
    constexpr int titleLeft = 100;
    constexpr int titleTop = 45;
    constexpr int titleHeight = 40;
    constexpr int titleWidth = 1000;

    IFPainter painter(Options::CANONICAL_REPORT_LENGTH, Options::CANONICAL_REPORT_WIDTH);
    const int pageRight = Options::CANONICAL_REPORT_WIDTH - pageMargin;
    painter.drawRect(pageMargin, pageMargin, pageRight, bannerBottom, -1, cv::Scalar(0, 0, 0));
    painter.drawRect(factsLeft, factsTop, pageRight, factsBottom, -1, cv::Scalar(220, 220, 220));
    painter.drawLine(dividerLeft, dividerY, dividerRight, dividerY, dividerWidth, cv::Scalar(94, 58, 32));
    painter.drawText(titleLeft, titleTop, titleHeight, titleWidth, "GIST layout", TO_WHITE | TO_BOLD);
    painter.exportToFile(fullPath.string());

    painter.scaleTo(options.getWidth(), options.getLength());
    painter.exportToFile(previewPath.string());

    const cv::Mat fullImage = cv::imread(fullPath.string(), cv::IMREAD_UNCHANGED);
    const cv::Mat previewImage = cv::imread(previewPath.string(), cv::IMREAD_UNCHANGED);
    if (fullImage.empty() || previewImage.empty()) {
	std::cerr << "Could not read generated test images\n";
	return 1;
    }

    cv::Mat expectedPreview;
    cv::resize(fullImage, expectedPreview, previewImage.size(), 0.0, 0.0, cv::INTER_AREA);

    std::error_code removeError;
    std::filesystem::remove(fullPath, removeError);
    if (removeError) {
	std::cerr << "Could not remove " << fullPath << ": " << removeError.message() << "\n";
	return 1;
    }
    std::filesystem::remove(previewPath, removeError);
    if (removeError) {
	std::cerr << "Could not remove " << previewPath << ": " << removeError.message() << "\n";
	return 1;
    }

    if (previewImage.cols != expectedWidth || previewImage.rows != expectedHeight) {
	std::cerr << "Preview image has incorrect dimensions\n";
	return 1;
    }
    if (!sameImage(expectedPreview, previewImage)) {
	std::cerr << "Preview image differs from the scaled full-resolution layout\n";
	return 1;
    }

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
