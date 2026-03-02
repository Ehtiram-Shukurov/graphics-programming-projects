#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES 
#endif
#include <omp.h>

#define STB_IMAGE_IMPLEMENTATION 
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "headers/image_lib.h"
#include <cstdlib>
#include "random"

//#Vec3 Library
#include "headers/vec3.h"

//High resolution timer
#include <chrono>

//Scene file parser
#include "headers/parse_vec3.h"

#include "headers/scene_structs.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

bool refract(const vec3& v, const vec3& n, float ior_ratio, vec3& out_refracted_dir) {
  vec3 v_copy = v;
  vec3 v_incident = v_copy.normalized();
  float dt = dot(v_incident, n);
  float discriminant = 1.0f - ior_ratio * ior_ratio * (1.0f - dt * dt);

  if (discriminant > 0.0f) {
    out_refracted_dir = ior_ratio * (v_incident - n * dt) - n * sqrtf(discriminant);
    out_refracted_dir = out_refracted_dir.normalized();
    return true;
  }
  return false; 
}

float rayTriangleIntersect(const vec3& ray_origin, const vec3& ray_dir,
const Triangle& tri,float& out_beta,float& out_gamma){

  const vec3& v0 = scene_vertices[tri.v_indices[0]];
  const vec3& v1 = scene_vertices[tri.v_indices[1]];
  const vec3& v2 = scene_vertices[tri.v_indices[2]];
  vec3 edge1 = v1 - v0;
  vec3 edge2 = v2 - v0;
  vec3 plane_normal = cross(edge1,edge2);
  vec3 h = cross(ray_dir,edge2);
  float a = dot(edge1,h);
  float epsilon = 0.00001f;
  if (a > -epsilon && a < epsilon){
    return std::numeric_limits<float>::infinity(); 
  }

  vec3 s = ray_origin - v0;
  float beta_numerator = dot(s,h);

  if (a > 0) {
    if (beta_numerator < 0.0f || beta_numerator > a)
      return std::numeric_limits<float>::infinity();
  } 
  else {
    if (beta_numerator > 0.0f || beta_numerator < a)
      return std::numeric_limits<float>::infinity();
  }
  vec3 q = cross(s, edge1);
  float gamma_numerator = dot(ray_dir, q);

  if (a > 0) {
    if (gamma_numerator < 0.0f || beta_numerator + gamma_numerator > a)
      return std::numeric_limits<float>::infinity();
  } else {
    if (gamma_numerator > 0.0f || beta_numerator + gamma_numerator < a)
      return std::numeric_limits<float>::infinity();
  }

  float f = 1.0f / a;
  float t = f * dot(edge2, q);

  if (t > epsilon) {
    out_beta = beta_numerator * f;
    out_gamma = gamma_numerator * f;
    return t;
  } 
  else {
    return std::numeric_limits<float>::infinity();
  }
}

float raySphereIntersect(vec3 start, vec3 dir, vec3 center, float radius){
  float a = dot(dir,dir); 
  vec3 toStart = (start - center);
  float b = 2 * dot(dir,toStart);
  float c = dot(toStart,toStart) - radius*radius;
  float discr = b*b - 4*a*c;
  
  float t0,t1;
  float infinity = std::numeric_limits<float>::infinity();


  if (discr < 0) {
    return infinity;
  }
  else{
    t0 = (-b + sqrt(discr))/(2*a);
    t1 = (-b - sqrt(discr))/(2*a);
    float epsilon = 0.001f;
    if (t1 > epsilon){
      return t1;
    }
    else if(t0 > epsilon){
      return t0;
    }
    else {
      return infinity;
    }
  }
}

float rayPlaneIntersect(const vec3& ray_origin, const vec3& ray_dir, const Plane& plane) {
  float epsilon = 0.00001f;
  float N_dot_V = dot(plane.normal, ray_dir);
  if (std::abs(N_dot_V) < epsilon) {
    return std::numeric_limits<float>::infinity();
  }
  float N_dot_P0 = dot(plane.normal, plane.position);
  float N_dot_O = dot(plane.normal, ray_origin);
  float t = (N_dot_P0 - N_dot_O) / N_dot_V;
  if (t > epsilon) {
    return t;
  }
  return std::numeric_limits<float>::infinity();
}

float rayBoxIntersect(const vec3& ray_origin, const vec3& ray_dir, const Box& box, vec3& out_normal) {
  float tmin = -std::numeric_limits<float>::infinity();
  float tmax = std::numeric_limits<float>::infinity();
  float epsilon = 0.00001f;

  if (std::abs(ray_dir.x) < epsilon) {
    if (ray_origin.x < box.min_corner.x || ray_origin.x > box.max_corner.x) {
      return std::numeric_limits<float>::infinity();
    }
  }
  else{
    float t1 = (box.min_corner.x - ray_origin.x) / ray_dir.x;
    float t2 = (box.max_corner.x - ray_origin.x) / ray_dir.x;
    vec3 n1 = vec3(-1, 0, 0); 
    vec3 n2 = vec3(1, 0, 0);
    if (t1 > t2) { 
      std::swap(t1, t2); 
      std::swap(n1, n2); 
    }
    if (t1 > tmin) { 
      tmin = t1; 
      out_normal = n1; 
    }
    if (t2 < tmax) { 
      tmax = t2; 
    }
  }

  if (std::abs(ray_dir.y) < epsilon) {
    if (ray_origin.y < box.min_corner.y || ray_origin.y > box.max_corner.y) {
      return std::numeric_limits<float>::infinity();
    }
  }
  else {
    float t1 = (box.min_corner.y - ray_origin.y) / ray_dir.y;
    float t2 = (box.max_corner.y - ray_origin.y) / ray_dir.y;
    vec3 n1 = vec3(0, -1, 0); 
    vec3 n2 = vec3(0, 1, 0);
    if (t1 > t2) { 
      std::swap(t1, t2); 
      std::swap(n1, n2); 
    }
    if (t1 > tmin) { 
      tmin = t1; 
      out_normal = n1; 
    }
    if (t2 < tmax) { 
      tmax = t2; 
    }
  }

  if (std::abs(ray_dir.z) < epsilon) {
    if (ray_origin.z < box.min_corner.z || ray_origin.z > box.max_corner.z) {
      return std::numeric_limits<float>::infinity();
    }
  } 
  else {
    float t1 = (box.min_corner.z - ray_origin.z) / ray_dir.z;
    float t2 = (box.max_corner.z - ray_origin.z) / ray_dir.z;
    vec3 n1 = vec3(0, 0, -1); 
    vec3 n2 = vec3(0, 0, 1);
    if (t1 > t2) { 
      std::swap(t1, t2); 
      std::swap(n1, n2); 
    }
    if (t1 > tmin) { 
      tmin = t1; 
      out_normal = n1; 
    }
    if (t2 < tmax) { 
      tmax = t2; 
    }
  }

  if (tmin > tmax || tmax < epsilon) {
    return std::numeric_limits<float>::infinity();
  }
  if (tmin > epsilon) {
    return tmin;
  } else {
    return tmax;
  }
}

// Recursive ray tracing function
vec3 traceRay(vec3 ray_origin, vec3 ray_dir, int depth){
  //finding closest sphere intersection
  HitInfo closest_hit;
  closest_hit.t = std::numeric_limits<float>::infinity();
  closest_hit.hit_sphere = nullptr;
  closest_hit.hit_triangle = nullptr;
  closest_hit.hit_plane = nullptr;
  closest_hit.hit_box = nullptr;

  for(int k = 0; k<scene_spheres.size();++k){
    float t = raySphereIntersect(ray_origin,ray_dir,scene_spheres[k].center,scene_spheres[k].radius);
    if(t < closest_hit.t){
      closest_hit.t = t;
      closest_hit.hit_sphere = &scene_spheres[k];
      closest_hit.hit_triangle = nullptr;
      closest_hit.point = ray_origin + t *ray_dir;
      closest_hit.normal = (closest_hit.point - closest_hit.hit_sphere->center).normalized();
      closest_hit.material = closest_hit.hit_sphere->material;
    }
  }

  for(int k = 0; k < scene_triangles.size();++k){
    float current_beta, current_gamma;
    float t = rayTriangleIntersect(ray_origin, ray_dir, scene_triangles[k], current_beta, current_gamma);
    if (t < closest_hit.t) {
      closest_hit.t = t;
      closest_hit.hit_triangle = &scene_triangles[k];
      closest_hit.beta = current_beta;
      closest_hit.gamma = current_gamma;
      closest_hit.hit_sphere = nullptr;
      closest_hit.point = ray_origin + t * ray_dir;
      closest_hit.material = closest_hit.hit_triangle->material;
  }
}

  for(int k = 0; k < scene_planes.size(); ++k) {
    float t = rayPlaneIntersect(ray_origin, ray_dir, scene_planes[k]);
    if (t < closest_hit.t) {
      closest_hit.t = t;
      closest_hit.hit_plane = &scene_planes[k];
      closest_hit.hit_sphere = nullptr;
      closest_hit.hit_triangle = nullptr;
      closest_hit.point = ray_origin + t * ray_dir;
      closest_hit.normal = scene_planes[k].normal;
      closest_hit.material = scene_planes[k].material;
    }
  }

  vec3 box_normal;
  for(int k = 0; k < scene_boxes.size(); ++k) {
    vec3 current_normal;
    float t = rayBoxIntersect(ray_origin, ray_dir, scene_boxes[k], current_normal);
    if (t < closest_hit.t) {
      closest_hit.t = t;
      closest_hit.hit_box = &scene_boxes[k];
      closest_hit.hit_plane = nullptr;
      closest_hit.hit_sphere = nullptr;
      closest_hit.hit_triangle = nullptr;
      closest_hit.point = ray_origin + t * ray_dir;
      closest_hit.normal = current_normal;
      closest_hit.material = scene_boxes[k].material;
    }
  }
  
  if(closest_hit.hit_sphere == nullptr && closest_hit.hit_triangle == nullptr && closest_hit.hit_plane == nullptr && closest_hit.hit_box == nullptr){
    return backgroundColor;
  }
  else {
    if (closest_hit.hit_triangle != nullptr) {
      if (closest_hit.hit_triangle->smooth) {
        float beta = closest_hit.beta;
        float gamma = closest_hit.gamma;
        float alpha = 1.0f - beta - gamma;
        const vec3& n0 = scene_normals[closest_hit.hit_triangle->n_indices[0]];
        const vec3& n1 = scene_normals[closest_hit.hit_triangle->n_indices[1]];
        const vec3& n2 = scene_normals[closest_hit.hit_triangle->n_indices[2]];

        closest_hit.normal = (alpha * n0 + beta * n1 + gamma * n2).normalized();
      }
    else {
      const vec3& v0 = scene_vertices[closest_hit.hit_triangle->v_indices[0]];
      const vec3& v1 = scene_vertices[closest_hit.hit_triangle->v_indices[1]];
      const vec3& v2 = scene_vertices[closest_hit.hit_triangle->v_indices[2]];
      closest_hit.normal = cross(v1 - v0, v2 - v0).normalized();
    }

    if (dot(closest_hit.normal, ray_dir) > 0.0f) {
      closest_hit.normal = -1.0f * closest_hit.normal;
    }
    } 
    else if(closest_hit.hit_plane != nullptr){
      if (dot(closest_hit.normal, ray_dir) > 0.0f) {
        closest_hit.normal = -1.0f * closest_hit.normal;
      }
    }
    else if (closest_hit.hit_box != nullptr) {
      if (dot(closest_hit.normal, ray_dir) > 0.0f) {
        closest_hit.normal = -1.0f * closest_hit.normal;
      }
    }
    vec3 local_color = closest_hit.material.ambient * ambient_light;
    
    //point lights
    for (int k = 0; k < scene_point_lights.size(); ++k){
      const PointLight& light = scene_point_lights[k];
      vec3 light_vec = light.position - closest_hit.point;
      float light_dist = light_vec.length();
      float atten = 1.0f / (light_dist * light_dist);
      vec3 light_dir = light_vec.normalized();

      //shadow check 
      bool in_shadow = false;
      vec3 shadow_ray_start = closest_hit.point + 0.001f * closest_hit.normal;
      vec3 shadow_ray_dir = light_dir;
      for ( int s_i = 0; s_i < scene_spheres.size(); ++s_i){
        float t_shadow = raySphereIntersect(shadow_ray_start,shadow_ray_dir,scene_spheres[s_i].center,scene_spheres[s_i].radius);
        if(t_shadow < light_dist){
          in_shadow = true;
          break;
        }
      }
      if (!in_shadow) {
        for (int p_i = 0; p_i < scene_planes.size(); ++p_i) {
          float t_shadow = rayPlaneIntersect(shadow_ray_start, shadow_ray_dir, scene_planes[p_i]);
          if (t_shadow < light_dist) { 
            in_shadow = true; 
            break; 
          }
        }
      }
      if (!in_shadow) {
        vec3 temp_norm;
        for (int b_i = 0; b_i < scene_boxes.size(); ++b_i) {
          float t_shadow = rayBoxIntersect(shadow_ray_start, shadow_ray_dir, scene_boxes[b_i], temp_norm);
          if (t_shadow < light_dist) { 
            in_shadow = true; 
            break; 
          }
        }
      }
      if (!in_shadow) {
        for (int t_i = 0; t_i < scene_triangles.size(); ++t_i) {
          float beta, gamma;
          float t_shadow = rayTriangleIntersect(shadow_ray_start, shadow_ray_dir, scene_triangles[t_i], beta, gamma);
          if (t_shadow < light_dist) {
            in_shadow = true;
            break;
          }
        }
      }
      if(!in_shadow){
        float N_dot_L = fmax(0.0f, dot(closest_hit.normal,light_dir));
        
        vec3 diffuse_term = closest_hit.material.diffuse * light.color * N_dot_L *atten;
        vec3 specular_term = vec3(0,0,0);

        if(N_dot_L > 0.0f){
          vec3 view_dir = (eye - closest_hit.point).normalized();
          vec3 reflect_dir = 2.0f * N_dot_L * closest_hit.normal -light_dir;
          float V_dot_R = dot(view_dir,reflect_dir.normalized());

          if(V_dot_R > 0.0f){
            float specular_intensity = pow(V_dot_R,closest_hit.material.phong_exponent);
            specular_term = closest_hit.material.specular * light.color *specular_intensity * atten;
          }
        }
        local_color = local_color + diffuse_term +specular_term;
      }
      }
    
    //directional lights
    for (int k = 0; k < scene_directional_lights.size(); ++k){
      const DirectionalLight& light = scene_directional_lights[k];
      vec3 light_dir = light.direction;
      bool in_shadow = false;
      vec3 shadow_ray_start = closest_hit.point + 0.001f * closest_hit.normal;
      vec3 shadow_ray_dir = light_dir;

      for (int s_i = 0; s_i < scene_spheres.size(); ++s_i){
        float t_shadow = raySphereIntersect(shadow_ray_start,shadow_ray_dir,scene_spheres[s_i].center,scene_spheres[s_i].radius);
        if (t_shadow < std::numeric_limits<float>::infinity()) { 
          in_shadow = true;
          break;
        }
      }
      if (!in_shadow) {
        for (int p_i = 0; p_i < scene_planes.size(); ++p_i) {
          float t_shadow = rayPlaneIntersect(shadow_ray_start, shadow_ray_dir, scene_planes[p_i]);
          if (t_shadow < std::numeric_limits<float>::infinity()) { 
            in_shadow = true; 
            break; 
          }
          }
      }
      if (!in_shadow) {
        vec3 temp_norm;
        for (int b_i = 0; b_i < scene_boxes.size(); ++b_i) {
          float t_shadow = rayBoxIntersect(shadow_ray_start, shadow_ray_dir, scene_boxes[b_i], temp_norm);
          if (t_shadow < std::numeric_limits<float>::infinity()) { 
            in_shadow = true; 
            break; 
          }
          }
      }
      if (!in_shadow) {
        for (int t_i = 0; t_i < scene_triangles.size(); ++t_i) {
          float beta, gamma;
          float t_shadow = rayTriangleIntersect(shadow_ray_start, shadow_ray_dir, scene_triangles[t_i], beta, gamma);
          if (t_shadow < std::numeric_limits<float>::infinity()) {
            in_shadow = true;
            break;
          }
        }
      }

      if (!in_shadow) {
        float N_dot_L = fmax(0.0f, dot(closest_hit.normal, light_dir));
        vec3 diffuse_term = closest_hit.material.diffuse * light.color * N_dot_L;
        vec3 specular_term = vec3(0,0,0);
        if (N_dot_L > 0.0f) { 
          vec3 view_dir = (eye - closest_hit.point).normalized();
          vec3 reflect_dir = 2.0f * N_dot_L * closest_hit.normal - light_dir;
          float V_dot_R = dot(view_dir, reflect_dir.normalized());
          if (V_dot_R > 0.0f) {
            float specular_intensity = pow(V_dot_R, closest_hit.material.phong_exponent);
            specular_term = closest_hit.material.specular * light.color * specular_intensity;
          }
        }
        local_color = local_color + diffuse_term + specular_term;
      }
    }
      
      //spot lights
    for (int k = 0; k < scene_spot_lights.size(); ++k){
      const SpotLight& light = scene_spot_lights[k];
      vec3 light_vec = light.position - closest_hit.point;
      float light_dist = light_vec.length();
      float atten = 1.0f / (light_dist * light_dist);
      vec3 light_dir = light_vec.normalized();

      float spot_intensity = 0.0f;
      float surface_dot_light = dot(light.direction, -1.0f * light_dir);
      float cos_angle1 = cosf(light.angle1 * M_PI / 180.0f);
      float cos_angle2 = cosf(light.angle2 * M_PI / 180.0f);

      if (surface_dot_light > cos_angle1) {
        spot_intensity = 1.0f;
      } 
      else if (surface_dot_light > cos_angle2) {
        spot_intensity = (surface_dot_light - cos_angle2) / (cos_angle1 - cos_angle2);
      }

      if (spot_intensity > 0.0f) {
        bool in_shadow = false;
        vec3 shadow_ray_start = closest_hit.point + 0.001f * closest_hit.normal;
        vec3 shadow_ray_dir = light_dir;
            
        for (int s_i = 0; s_i < scene_spheres.size(); ++s_i) {
          float t_shadow = raySphereIntersect(shadow_ray_start, shadow_ray_dir, scene_spheres[s_i].center, scene_spheres[s_i].radius);
          if (t_shadow < light_dist) { 
            in_shadow = true; 
            break; 
          }
        }
        if (!in_shadow) {
          for (int t_i = 0; t_i < scene_triangles.size(); ++t_i) {
            float beta, gamma;
            float t_shadow = rayTriangleIntersect(shadow_ray_start, shadow_ray_dir, scene_triangles[t_i], beta, gamma);
            if (t_shadow < light_dist) { 
              in_shadow = true; 
              break; 
            }
          }
        }
        if (!in_shadow) {
          for (int p_i = 0; p_i < scene_planes.size(); ++p_i) {
            float t_shadow = rayPlaneIntersect(shadow_ray_start, shadow_ray_dir, scene_planes[p_i]);
            if (t_shadow < light_dist) { 
              in_shadow = true; 
              break; 
            }
          }
        }
        if (!in_shadow) {
          vec3 temp_norm;
          for (int b_i = 0; b_i < scene_boxes.size(); ++b_i) {
            float t_shadow = rayBoxIntersect(shadow_ray_start, shadow_ray_dir, scene_boxes[b_i], temp_norm);
            if (t_shadow < light_dist) { 
              in_shadow = true; 
              break; 
            }
          }
        }
        if (!in_shadow) {
          float N_dot_L = fmax(0.0f, dot(closest_hit.normal, light_dir));
          vec3 diffuse_term = closest_hit.material.diffuse * light.color * N_dot_L * atten * spot_intensity;
          vec3 specular_term = vec3(0, 0, 0);

          if (N_dot_L > 0.0f) {
            vec3 view_dir = (eye - closest_hit.point).normalized();
            vec3 reflect_dir = 2.0f * N_dot_L * closest_hit.normal - light_dir;
            float V_dot_R = dot(view_dir, reflect_dir.normalized());
            if (V_dot_R > 0.0f) {
              float specular_intensity = pow(V_dot_R, closest_hit.material.phong_exponent);
              specular_term = closest_hit.material.specular * light.color * specular_intensity * atten * spot_intensity;
            }
          }
          local_color = local_color + diffuse_term + specular_term;
        }
        }
      }

        //calculating reflection
    vec3 reflected_color = vec3(0,0,0);
    float spec_mag_sq = dot(closest_hit.material.specular,closest_hit.material.specular);
    if (depth < max_recursion_depth && spec_mag_sq > 0.001f) {
      vec3 incoming_dir = ray_dir;
      vec3 reflect_dir_ray = incoming_dir - 2.0f * dot(incoming_dir, closest_hit.normal) * closest_hit.normal;
      vec3 reflection_origin = closest_hit.point + 0.001f * reflect_dir_ray.normalized(); 
      reflected_color = traceRay(reflection_origin, reflect_dir_ray.normalized(), depth + 1);
    }
    vec3 refracted_color = vec3(0,0,0);
    float trans_mag_sq = dot(closest_hit.material.transmissive, closest_hit.material.transmissive);
      
    if (depth < max_recursion_depth && trans_mag_sq > 0.001f) {
      float ior_from = 1.0f;
      float ior_to = closest_hit.material.ior;
      vec3 normal = closest_hit.normal;
      if (dot(ray_dir, normal) > 0.0f) { 
        std::swap(ior_from, ior_to);
        normal = -1.0f * normal; 
      }
      float ior_ratio = ior_from / ior_to;
      vec3 refracted_dir;

      if (refract(ray_dir, normal, ior_ratio, refracted_dir)) {
        vec3 refraction_origin = closest_hit.point + 0.001f * refracted_dir.normalized();
        refracted_color = traceRay(refraction_origin, refracted_dir.normalized(), depth + 1);
      }
    }

    vec3 final_color_vec = local_color + closest_hit.material.specular * reflected_color
    + closest_hit.material.transmissive * refracted_color;
    final_color_vec = final_color_vec.clampTo1();
    return final_color_vec;
  }
}

int main(int argc, char** argv){
  //Read command line paramaters to get scene file
  if (argc != 2){
     std::cout << "Usage: ./a.out scenefile\n";
     return(0);
  }
  std::string secenFileName = argv[1];

  //Parse Scene File
  parseSceneFile(secenFileName);

  float imgW = img_width, imgH = img_height;
  float halfW = imgW/2, halfH = imgH/2;
  float aspect_ratio = imgW / imgH; 
  float vfov_rad = halfAngleVFOV * (M_PI / 180.0f);
  float scale_y = tanf(vfov_rad); 
  float scale_x = scale_y * aspect_ratio; 
  float d = halfH / tanf(vfov_rad);

  Image outputImg = Image(img_width,img_height);
  auto t_start = std::chrono::high_resolution_clock::now();
  #pragma omp parallel for
  
  //Sampling
  for (int i = 0; i < img_width; i++){
    for (int j = 0; j < img_height; j++){
      vec3 accumulated_color = vec3(0,0,0);
      for (int s = 0; s< num_samples; ++s){
        //generating jittered ray
        float dx = (float)rand() / (float)RAND_MAX -0.5f;
        float dy = (float)rand() / (float)RAND_MAX -0.5f;

        float Px = (2.0f * (i + 0.5f + dx) / imgW - 1.0f);
        float Py = (1.0f - 2.0f * (j + 0.5f + dy) / imgH);

        vec3 u_offset_vec = Px * scale_x * d * right; 
        vec3 v_offset_vec = Py * scale_y * d * up; 

        vec3 p_jittered = eye - d * forward + u_offset_vec + v_offset_vec;
        vec3 rayDir_jittered = (p_jittered - eye).normalized();
        accumulated_color = accumulated_color + traceRay(eye, rayDir_jittered, 0);
        }

      vec3 final_color_vec = accumulated_color *(1.0f / num_samples);
      final_color_vec = final_color_vec.clampTo1();
      Color pixelColor = Color(final_color_vec.x, final_color_vec.y, final_color_vec.z);
      outputImg.setPixel(i, j, pixelColor);
    }
  }
  auto t_end = std::chrono::high_resolution_clock::now();
  printf("Rendering took %.2f ms\n",std::chrono::duration<double, std::milli>(t_end-t_start).count());

  outputImg.write(imgName.c_str());
  return 0;
}