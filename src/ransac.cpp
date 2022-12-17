#include "ransac.h"

#include <math.h> /* sqrt & pow*/

#include <Eigen/Geometry>
#include <cmath>
#include <numeric>

namespace tnp {

std::vector<float> insert_sorted(std::vector<float> vector, float value) {
  if (vector.size() == 0) return {value};

  for (uint i = 0; i < vector.size(); i++) {
    if (value < vector[i]) {
      vector.insert(vector.begin() + i, value);
      break;
    }
  }
  return vector;
}

float dist_mean_closest_neighbors(std::vector<Eigen::Vector3f> cloud,
                                  std::vector<uint> points, uint base_point,
                                  uint k) {
  uint points_size = points.size();

  std::vector<float> closest_neighbors;

  for (uint i = 0; i < points_size; i++) {
    if (points[i] == base_point) continue;
    float dist_with_point = (cloud[base_point] - cloud[points[i]]).norm();

    if (closest_neighbors.size() < k) {
      closest_neighbors = insert_sorted(closest_neighbors, dist_with_point);
      continue;
    }

    // ELSE : the vector of neighbor has its maximum size.
    if (closest_neighbors.back() > dist_with_point) {
      closest_neighbors = insert_sorted(closest_neighbors, dist_with_point);
      closest_neighbors.pop_back();
    }
  }

  float mean =
      std::accumulate(closest_neighbors.begin(), closest_neighbors.end(), 0);
  mean /= closest_neighbors.size();
  return mean;
}

std::pair<std::vector<uint>, std::vector<uint>> outliers_removal(
    std::vector<Eigen::Vector3f> cloud, std::vector<uint> inliers,
    std::vector<uint> remaining_point_cloud) {
  std::vector<std::pair<uint, float>> points_d_mean;

  uint inliers_size = inliers.size();

  float mean = 0;
  for (uint i = 0; i < inliers_size; i++) {
    float dist_mean =
        dist_mean_closest_neighbors(cloud, inliers, inliers[i], 200);

    // Saves the point and its d_mean distance
    std::pair<uint, float> point_d_mean(inliers[i], dist_mean);
    points_d_mean.push_back(point_d_mean);
    mean += point_d_mean.second;
  }
  mean /= (float)points_d_mean.size();

  float standard_deviation = 0;
  for (std::pair<uint, float> point_d_mean : points_d_mean)
    standard_deviation += pow(point_d_mean.second - mean, 2);
  standard_deviation = sqrt(standard_deviation / (float)points_d_mean.size());

  float alpha = 1;  // Similarity factor
  std::vector<uint> new_inliers;
  for (std::pair<uint, float> point_d_mean : points_d_mean) {
    if ((mean - alpha * standard_deviation) <= point_d_mean.second &&
        point_d_mean.second <= (mean + alpha * standard_deviation))
      new_inliers.push_back(point_d_mean.first);
    else
      remaining_point_cloud.push_back(point_d_mean.first);
  }
  return {new_inliers, remaining_point_cloud};
}

std::pair<std::vector<uint>, std::vector<uint>> ransac(
    const std::vector<Eigen::Vector3f>& points, const float threshold,
    const uint max_number_of_iterations, bool remove_outliers) {
  std::vector<uint> best_inliers;
  std::vector<uint> best_outliers;

  for (uint k = 0; k < max_number_of_iterations; k++) {
    uint a = std::rand() % points.size();
    uint b = std::rand() % points.size();
    uint c = std::rand() % points.size();

    // Create Eigen plane
    Eigen::Hyperplane<float, 3> plane =
        Eigen::Hyperplane<float, 3>::Through(points[a], points[b], points[c]);

    std::vector<uint> inliers;
    std::vector<uint> outliers;

    // Find inliers
    for (uint i = 0; i < points.size(); i++) {
      if (plane.absDistance(points[i]) <= threshold)
        inliers.push_back(i);
      else
        outliers.push_back(i);
    }

    // Check if new plane has more inliers
    if (inliers.size() > best_inliers.size()) {
      best_inliers = inliers;
      best_outliers = outliers;
    }
  }


  if (remove_outliers)
    return outliers_removal(points, best_inliers, best_outliers);
  return {best_inliers, best_outliers};
}

std::vector<std::vector<Eigen::Vector3f>> ransac_multi(
    const std::vector<Eigen::Vector3f>& points, const float threshold,
    const uint max_number_of_iterations, const uint max_objects,
    const float min_inliers_ratio, bool remove_outliers) {
  std::vector<std::vector<Eigen::Vector3f>> objects;
  std::vector<Eigen::Vector3f> remaining_points = points;

  float inliers_ratio = 1.0;

  while (objects.size() < max_objects && inliers_ratio >= min_inliers_ratio) {
    if (remaining_points.size() == 0) return objects;

    std::pair<std::vector<uint>, std::vector<uint>> indexes =
        ransac(remaining_points, threshold, max_number_of_iterations, remove_outliers);

    std::vector<Eigen::Vector3f> inliers;
    std::vector<Eigen::Vector3f> outliers;
    for (uint i : indexes.first) {
      inliers.push_back(remaining_points[i]);
    }

    for (uint i : indexes.second) {
      outliers.push_back(remaining_points[i]);
    }

    inliers_ratio = float(inliers.size()) / points.size();

    if (inliers_ratio >= min_inliers_ratio) {
      objects.push_back(inliers);
      remaining_points = outliers;
    }
  }

  objects.push_back(remaining_points);
  return objects;
}

}  // namespace tnp