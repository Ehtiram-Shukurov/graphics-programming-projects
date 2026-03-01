#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES 
#endif

//Images Lib includes:
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

//Testing if the ray intersects the sphere
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


// Recursive ray tracing function
vec3 traceRay(vec3 ray_origin, vec3 ray_dir, int depth){
  //finding closest sphere intersection
  HitInfo closest_hit;
  closest_hit.t = std::numeric_limits<float>::infinity();
  closest_hit.hit_sphere = nullptr;

  for(int k = 0; k<scene_spheres.size();++k){
    float t = raySphereIntersect(ray_origin,ray_dir,scene_spheres[k].center,scene_spheres[k].radius);
    if(t < closest_hit.t){
      closest_hit.t = t;
      closest_hit.hit_sphere = &scene_spheres[k];
      closest_hit.point = ray_origin + t *ray_dir;
      closest_hit.normal = (closest_hit.point - closest_hit.hit_sphere->center).normalized();
    }
  }

    if(closest_hit.hit_sphere == nullptr){
      return backgroundColor;
    }
    else {
      vec3 local_color = closest_hit.hit_sphere->material.ambient * ambient_light;
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
          if(!in_shadow){
          float N_dot_L = fmax(0.0f, dot(closest_hit.normal,light_dir));
          
          vec3 diffuse_term = closest_hit.hit_sphere->material.diffuse * light.color * N_dot_L *atten;
          vec3 specular_term = vec3(0,0,0);

          if(N_dot_L > 0.0f){
            vec3 view_dir = (eye - closest_hit.point).normalized();
            vec3 reflect_dir = 2.0f * N_dot_L * closest_hit.normal -light_dir;
            float V_dot_R = dot(view_dir,reflect_dir.normalized());

            if(V_dot_R > 0.0f){
              float specular_intensity = pow(V_dot_R,closest_hit.hit_sphere->material.phong_exponent);
              specular_term = closest_hit.hit_sphere->material.specular * light.color *specular_intensity * atten;
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
        float N_dot_L = fmax(0.0f, dot(closest_hit.normal, light_dir));
        vec3 diffuse_term = closest_hit.hit_sphere->material.diffuse * light.color * N_dot_L;
        vec3 specular_term = vec3(0,0,0);
         if (N_dot_L > 0.0f) { 
            vec3 view_dir = (eye - closest_hit.point).normalized();
            vec3 reflect_dir = 2.0f * N_dot_L * closest_hit.normal - light_dir;
            float V_dot_R = dot(view_dir, reflect_dir.normalized());
            if (V_dot_R > 0.0f) {
              float specular_intensity = pow(V_dot_R, closest_hit.hit_sphere->material.phong_exponent);
              specular_term = closest_hit.hit_sphere->material.specular * light.color * specular_intensity;
            }
        }
        local_color = local_color + diffuse_term + specular_term;
    }
        }
        //calculating reflection
        vec3 reflected_color = vec3(0,0,0);
        float spec_mag_sq = dot(closest_hit.hit_sphere->material.specular,closest_hit.hit_sphere->material.specular);
        if (depth < max_recursion_depth && spec_mag_sq > 0.001f) {

            vec3 incoming_dir = ray_dir;
            vec3 reflect_dir_ray = incoming_dir - 2.0f * dot(incoming_dir, closest_hit.normal) * closest_hit.normal;

            vec3 reflection_origin = closest_hit.point + 0.001f * reflect_dir_ray.normalized(); 

            reflected_color = traceRay(reflection_origin, reflect_dir_ray.normalized(), depth + 1);
        }
        vec3 final_color_vec = local_color + closest_hit.hit_sphere->material.specular *reflected_color;

        final_color_vec = final_color_vec.clampTo1();
        return final_color_vec;
    }
  }

int main(int argc, char** argv){

  //Read command line paramaters to get scene file
  if (argc != 2){
     std::cout << "Usage: raytracer <scene_file>\n"
          << "Example: raytracer scenes/bear.txt\n";
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
  
  //Sampling
  for (int i = 0; i < img_width; i++){
    for (int j = 0; j < img_height; j++){
      vec3 accumulated_color = vec3(0,0,0);
      if(num_samples == 1){
        float Px = (2.0f * (i + 0.5f) / imgW - 1.0f);
        float Py = (1.0f - 2.0f * (j + 0.5f) / imgH);

        vec3 u_offset_vec = Px * scale_x * d * right; 
        vec3 v_offset_vec = Py * scale_y * d * up; 

        vec3 p = eye - d * forward + u_offset_vec + v_offset_vec;
        vec3 rayDir = (p - eye).normalized();
        accumulated_color = traceRay(eye, rayDir, 0);
      }
      else{
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