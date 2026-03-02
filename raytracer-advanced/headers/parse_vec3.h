#ifndef PARSE_VEC3_H
#define PARSE_VEC3_H

#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <limits>
#include <cmath>
#include "scene_structs.h"

//Camera & Scene Parameters (Global Variables)
//Here we set default values, override them in parseSceneFile()

//Image Parameters
int img_width = 640, img_height = 480;
std::string imgName = "raytraced.png";

//Camera Parameters
vec3 eye = vec3(0,0,0); 
vec3 forward = vec3(0,0,-1).normalized();
vec3 up = vec3(0,1,0).normalized();
vec3 right;
float halfAngleVFOV = 45; 

//Scene Parameters
int num_samples = 16;
vec3 backgroundColor = vec3(0,0,0);
int max_recursion_depth = 5;

std::vector<Sphere> scene_spheres;
std::vector<PointLight> scene_point_lights;
std::vector<DirectionalLight> scene_directional_lights;
std::vector<vec3> scene_vertices;
std::vector<vec3> scene_normals;
std::vector<Triangle> scene_triangles;
std::vector<SpotLight> scene_spot_lights;

std::vector<Plane> scene_planes;
std::vector<Box> scene_boxes;

vec3 ambient_light = vec3(0,0,0);

void parseSceneFile(std::string fileName){
  std::ifstream input(fileName.c_str());

  if(input.fail()){
    std::cout<< "Can't open file " << fileName << std::endl;
    return; 
  }

  Material current_material;
  scene_spheres.clear();
  scene_point_lights.clear();
  scene_directional_lights.clear();
  scene_vertices.clear();
  scene_normals.clear();
  scene_triangles.clear();
  scene_spot_lights.clear();
  scene_boxes.clear();

  std::string command;
  std::string line;
  while(input >> command){
    if(command[0] == '#'){
      std::getline(input, line);
      continue;
    }
    if (command == "sphere:"){
      Sphere new_sphere;
      input >> new_sphere.center.x >> new_sphere.center.y 
      >> new_sphere.center.z >> new_sphere.radius;
      new_sphere.material = current_material;
      scene_spheres.push_back(new_sphere);
    }
    else if (command == "point_light:"){
      PointLight new_light;
      input >> new_light.color.x >> new_light.color.y >> new_light.color.z >> new_light.position.x 
      >> new_light.position.y >> new_light.position.z;
      scene_point_lights.push_back(new_light);
    }
    else if(command == "background:"){
      input >> backgroundColor.x >> backgroundColor.y >> backgroundColor.z;
    }
    else if (command == "film_resolution:"){
      input >> img_width >> img_height;
    }
    else if (command == "output_image:"){
      input >> imgName;
    }
    else if (command == "camera_pos:"){
      input >> eye.x >> eye.y >> eye.z;
    }
    else if (command == "camera_fwd:"){
      input >> forward.x >> forward.y >> forward.z;
    }
    else if (command == "camera_up:"){
      input >> up.x >> up.y >> up.z;
    }
    else if (command == "camera_fov_ha:"){
      input >> halfAngleVFOV;
    }
    else if (command == "material:"){
      input >> current_material.ambient.x >> current_material.ambient.y >> current_material.ambient.z
      >> current_material.diffuse.x >> current_material.diffuse.y >> current_material.diffuse.z;
      input >> current_material.specular.x >> current_material.specular.y >> current_material.specular.z >> current_material.phong_exponent;
      input >> current_material.transmissive.x >> current_material.transmissive.y >> current_material.transmissive.z >> current_material.ior;

      std::getline(input, line);
    } 
    else if (command == "ambient_light:") {
      input >> ambient_light.x >> ambient_light.y >> ambient_light.z;
    }

    else if(command == "directional_light:"){
      DirectionalLight new_light;
      vec3 dir_to;
      input >> new_light.color.x >> new_light.color.y >> new_light.color.z >> dir_to.x >> dir_to.y >> dir_to.z;
      new_light.direction = (-1.0f * dir_to).normalized();
      scene_directional_lights.push_back(new_light);
    }

    else if(command == "max_depth:"){
      input >> max_recursion_depth;
    }
    //to switch sampling
    else if (command == "samples_per_pixel:") {
      input >> num_samples;
      if (num_samples < 1) num_samples = 1;
    }
    else if(command == "max_vertices:"){
      int n;
      input >> n;
      if (n>0){
        scene_vertices.reserve(n);
        std::getline(input,line);
      }
    }
    else if(command == "max_normals:"){
      int n;
      input >> n;
      if (n >0) {
        scene_normals.reserve(n);
      }
      std::getline(input,line);
    }
    else if(command == "vertex:"){
      vec3 new_vertex;
      input >> new_vertex.x >> new_vertex.y >> new_vertex.z;
      scene_vertices.push_back(new_vertex);
      std::getline(input,line);

    }
    else if(command == "normal:"){
      vec3 new_normal;
      input >> new_normal.x >> new_normal.y >> new_normal.z;
      scene_normals.push_back(new_normal.normalized());
      std::getline(input,line);
    }
    else if(command == "triangle:"){
      Triangle new_triangle;
      input >> new_triangle.v_indices[0] >> new_triangle.v_indices[1] >> new_triangle.v_indices[2];
      new_triangle.material = current_material;
      new_triangle.smooth = false;
      new_triangle.n_indices[0] = new_triangle.n_indices[1] = new_triangle.n_indices[2] = -1;
      scene_triangles.push_back(new_triangle);
      std::getline(input,line);
    }
    else if(command == "normal_triangle:"){
      Triangle new_triangle;
      input >> new_triangle.v_indices[0] >> new_triangle.v_indices[1] >> new_triangle.v_indices[2]
              >> new_triangle.n_indices[0] >> new_triangle.n_indices[1] >> new_triangle.n_indices[2];
      new_triangle.material = current_material;
      new_triangle.smooth = true;
      scene_triangles.push_back(new_triangle);
      std::getline(input,line);      
    }
    else if(command == "spot_light:"){
      SpotLight new_light;
      input >> new_light.color.x >> new_light.color.y >> new_light.color.z >>
            new_light.position.x >> new_light.position.y >> new_light.position.z 
            >> new_light.direction.x >> new_light.direction.y >> new_light.direction.z >> new_light.angle1 >> new_light.angle2;
      new_light.direction = new_light.direction.normalized();
      scene_spot_lights.push_back(new_light);
      std::getline(input,line);
    }
    else if(command == "plane:"){

      Plane new_plane;
      input >> new_plane.position.x >> new_plane.position.y >> new_plane.position.z
            >> new_plane.normal.x >> new_plane.normal.y >> new_plane.normal.z;
      new_plane.normal = new_plane.normal.normalized();
      new_plane.material = current_material;
      scene_planes.push_back(new_plane);
      std::getline(input, line);
    }
    else if(command == "box:"){
      Box new_box;
      input >> new_box.min_corner.x >> new_box.min_corner.y >> new_box.min_corner.z
            >> new_box.max_corner.x >> new_box.max_corner.y >> new_box.max_corner.z;
      new_box.material = current_material;
      scene_boxes.push_back(new_box);
      std::getline(input, line);
    }
    else{
      std::getline(input,line);
      std::cout << "WARNING. Unknown command: " << command << std::endl;
    }
  }
  forward = forward.normalized();
  vec3 testUp = up;  //to check for parallelism
  if(testUp.length() == 0.0f || cross(forward,testUp).length() == 0.0f){
    if (std::abs(forward.y)< 0.999f){
      testUp =  vec3(0,1,0);
    }
    else{
      testUp = vec3(1,0,0);
    }
  }
  right = cross(forward,testUp).normalized();
  up = cross(right,forward).normalized();

  printf("Orthogonal Camera Basis:\n");
  printf("forward: %f,%f,%f\n",forward.x,forward.y,forward.z);
  printf("right: %f,%f,%f\n",right.x,right.y,right.z);
  printf("up: %f,%f,%f\n",up.x,up.y,up.z);
  
}

#endif