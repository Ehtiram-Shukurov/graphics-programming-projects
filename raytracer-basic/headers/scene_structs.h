#ifndef SCENE_STRUCTS_H
#define SCENE_STRUCTS_H

#include "vec3.h"
#include "limits"

struct Material {
    vec3 ambient = vec3(0,0,0);
    vec3 diffuse = vec3(1,1,1);
    vec3 specular = vec3(0,0,0);
    float phong_exponent = 5.0f;
};

struct Sphere {
    vec3 center;
    float radius;
    Material material; 
};

struct PointLight {
    vec3 position;
    vec3 color;
};

struct HitInfo {
  float t = std::numeric_limits<float>::infinity();
  vec3 point;
  vec3 normal;
  const Sphere* hit_sphere = nullptr;
};

struct DirectionalLight{
    vec3 direction;
    vec3 color;
};

#endif