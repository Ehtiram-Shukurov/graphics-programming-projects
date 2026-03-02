#ifndef SCENE_STRUCTS_H
#define SCENE_STRUCTS_H

#include "vec3.h"
#include "limits"

struct Material {
    vec3 ambient = vec3(0,0,0);
    vec3 diffuse = vec3(1,1,1);
    vec3 specular = vec3(0,0,0);
    float phong_exponent = 5.0f;

    vec3 transmissive = vec3(0,0,0);
    float ior = 1.0f;
};

struct Triangle {
    int v_indices[3];
    int n_indices[3];
    Material material;
    vec3 computed_normal;
    bool smooth;
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
struct Plane {
    vec3 normal;
    vec3 position;
    Material material;
};

struct Box {
    vec3 min_corner;
    vec3 max_corner;
    Material material;
};
struct HitInfo {
  float t = std::numeric_limits<float>::infinity();
  vec3 point;
  vec3 normal;
  const Sphere* hit_sphere = nullptr;
  Material material;
  const Triangle* hit_triangle = nullptr;
  float beta = 0.0f;
  float gamma = 0.0f;
  const Plane* hit_plane = nullptr;
  const Box* hit_box = nullptr;
};

struct DirectionalLight{
    vec3 direction;
    vec3 color;
};

struct SpotLight {
    vec3 position;
    vec3 color;
    vec3 direction;
    float angle1;
    float angle2;
};



extern std::vector<vec3> scene_vertices;
extern std::vector<vec3> scene_normals;
extern std::vector<Triangle> scene_triangles;
extern std::vector<SpotLight> scene_spot_lights;
extern std::vector<Plane> scene_planes;
extern std::vector<Box> scene_boxes;


#endif