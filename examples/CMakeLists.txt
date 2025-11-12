cmake_minimum_required(VERSION 3.15)

# 2D Collision Detection Example
add_executable(collision_detection_2d collision_detection_2d.cpp)
target_link_libraries(collision_detection_2d PRIVATE sparse_spatial_hash::sparse_spatial_hash)

# 3D Molecular Dynamics Example
add_executable(molecular_dynamics_3d molecular_dynamics_3d.cpp)
target_link_libraries(molecular_dynamics_3d PRIVATE sparse_spatial_hash::sparse_spatial_hash)

# Toroidal World Example
add_executable(toroidal_world toroidal_world.cpp)
target_link_libraries(toroidal_world PRIVATE sparse_spatial_hash::sparse_spatial_hash)

# Multi-Resolution Grid Example
add_executable(multi_resolution multi_resolution.cpp)
target_link_libraries(multi_resolution PRIVATE sparse_spatial_hash::sparse_spatial_hash)
