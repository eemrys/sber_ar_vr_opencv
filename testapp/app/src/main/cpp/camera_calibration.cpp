#include "camera_calibration.h"

#include <utility>

void cameracalibration::set_sizes(const Size& board, const Size& image, int square) {
    board_size = board;
    image_size = image;
    square_size = square;
}

int cameracalibration::identify_chessboard(Mat& frame, bool mode_take_snapshot) {

    vector<Point2f> corners;
    corners.clear();
    Mat gray;
    cvtColor(frame, gray, COLOR_BGR2GRAY);

    bool pattern_found = findChessboardCorners(gray, board_size, corners,
                                              CALIB_CB_ADAPTIVE_THRESH + CALIB_CB_NORMALIZE_IMAGE + CALIB_CB_FAST_CHECK);

    if (pattern_found) {
        cornerSubPix(gray, corners, Size(11, 11),
                     Size(-1, -1),
                     TermCriteria(TermCriteria::EPS + TermCriteria::COUNT, 30, 0.1));
        if (mode_take_snapshot)
        {
            if (image_points.size() < 20) {
                image_points.push_back(corners);
            }
        }
    }
    drawChessboardCorners(frame, board_size, Mat(corners), pattern_found);

    return image_points.size();
}

void cameracalibration::calc_board_corner_positions(vector<Point3f>& obj) {
    obj.clear();
    for (int i = 0; i < board_size.height; ++i)
        for (int j = 0; j < board_size.width; ++j)
            obj.emplace_back(j * square_size, i * square_size, 0);
}

vector<Mat> cameracalibration::calibrate() {
    float grid_width = (float)square_size * (board_size.width - 1.f);

    vector<vector<Point3f>> object_points(1);
    calc_board_corner_positions(object_points[0]);
    object_points[0][board_size.width - 1].x = object_points[0][0].x + grid_width;
    object_points.resize(image_points.size(), object_points[0]);

    Mat camera_matrix = Mat::eye(3, 3, CV_64F);
    Mat dist_coeffs = Mat::zeros(8, 1, CV_64F);

    vector<Mat> r_vecs, t_vecs;
    calibrateCamera(object_points, image_points, image_size,
                    camera_matrix, dist_coeffs, r_vecs, t_vecs);

    vector<Mat> results {camera_matrix, dist_coeffs};
    return results;
}

vector<double> cameracalibration::detect_aruco_marker(Mat& frame, const Mat& matrix, const Mat& dist) {
    double distance_marker = 0;
    double distance_surface = 0;
    float marker_length = 0.05f;

    vector<int> marker_ids;
    vector<vector<Point2f>> marker_corners;

    Ptr<aruco::DetectorParameters> parameters = aruco::DetectorParameters::create();
    parameters->cornerRefinementMethod = aruco::CORNER_REFINE_CONTOUR;
    parameters->adaptiveThreshConstant=true;
    Ptr<aruco::Dictionary> dictionary = aruco::getPredefinedDictionary(aruco::DICT_6X6_250);
    cvtColor(frame, frame, COLOR_RGBA2BGR);

    aruco::detectMarkers(frame, dictionary, marker_corners, marker_ids, parameters);

    if (!marker_ids.empty()) {
        aruco::drawDetectedMarkers(frame, marker_corners, marker_ids);

        vector<Vec3d> r_vecs, t_vecs;
        aruco::estimatePoseSingleMarkers(marker_corners, marker_length, matrix, dist, r_vecs, t_vecs);

        // draw axis for each marker (we only have one in this app)
        for (int i = 0; i < marker_ids.size(); ++i) {
            aruco::drawAxis(frame, matrix, dist, r_vecs[i], t_vecs[i], marker_length * 2.f);

            distance_marker = norm(t_vecs[i]);

            Mat rotation_matrix;
            Rodrigues(r_vecs[i], rotation_matrix);
            Mat camera_translation_vector = -rotation_matrix.t()*t_vecs[i];

            distance_surface = camera_translation_vector.at<double>(0,2);
        }
    }

    cvtColor(frame, frame, COLOR_BGR2RGB);

    vector<double> results {distance_marker, distance_surface};
    return results;
}