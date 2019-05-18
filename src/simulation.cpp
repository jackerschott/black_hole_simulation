#include <chrono>
#include <climits>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <time.h>
#include <utility>
#include <vector>

#include "gl.hpp"
#include "simulation.hpp"

#define PI      3.14159265358979323846
#define PI2     6.28318530717958647693
#define PI_2    1.57079632679489661923

// This improvisational install path is only working if the binary is executed from workspace/project folder!
#define INSTALL_PATH "./"
#define SHADER_PATH INSTALL_PATH "src/shader/"
#define TMP_PATH INSTALL_PATH "tmp/"

namespace sl {
  // GPU Programs
  gl::program* prog_bgmesh;
  gl::program* prog_rngUniform;
  gl::program* prog_rngGauss;
  gl::program* prog_clusterpos;
  gl::program* prog_clustercol;
  gl::program* prog_genBgTex;
  gl::program* prog_renderBg;
  gl::program* prog_renderCluster;
  gl::program* prog_test;

  // Cluster data
  int sCluster = -1;

  std::vector<uint> nClusterVerts;
  std::vector<GLuint> glClusterVerts;
  std::vector<GLuint> glClusterPosBufs;
  std::vector<GLuint> glClusterColBufs;
  GLuint glSelPts;

  // Camera data
  camera cam;
  glm::mat4 P;
  glm::mat2 P_I;
  glm::mat4 V;

  // Background data
  uint bgTexWidth;
  uint bgTexHeight;
  std::vector<char>* bgTexData;

  GLuint glBgVerts;
  GLuint glBgPosBuf;
  GLuint glBgElementBuf;
  GLuint glBgTex;
  GLuint glBgRenderTex;

  // Organisation Details
  slConfig config;

  void buildTestProg();

  uint64 getRngOff();
  void buildClusterProg();
  void buildBgProg();
  void buildUniformRngCompProg();
  void buildGaussRngCompProg();
  void buildClusterPosCompProg();
  void buildClusterColCompProg();
  bool readShaderSrc(const char* path, char** src, int* srcLen);
  void writeShaderSrc(const char* path, char* src, int srcLen);
  void GLAPIENTRY msgCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar* message, const void* userParam);
  const char* getErrSource(GLenum source);
  const char* getErrType(GLenum type);
  const char* getErrSeverity(GLenum severity);

  bool isClosed() {
    return (config & CONFIG_CLOSED) != 0;
  }
  bool isInit() {
    return (config & CONFIG_INIT) != 0;
  }
  bool hasCamera() {
    return (config & CONFIG_HAS_CAM) != 0;
  }
  bool hasBgTex() {
    return true; // (config & CONFIG_HAS_BG_TEX) != 0;
  }

  void init() {
    assert(config == CONFIG_CLOSED);

    // Initialize OpenGL
    glewInit();
    #if _DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(msgCallback, 0);
    #endif
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Create shaders from files
    // Cluster generation shaders
    gl::shader cs_rngUniform(GL_COMPUTE_SHADER, SHADER_PATH "rng/rng_uniform.comp");
    cs_rngUniform.setIncludeSrc("uint64.glsl", SHADER_PATH "lib/uint64.glsl");
    cs_rngUniform.setIncludeSrc("rng_mwc_skip.glsl", SHADER_PATH "rng/rng_mwc_skip.glsl");
    cs_rngUniform.setIncludeSrc("rng_mwc64x.glsl", SHADER_PATH "rng/rng_mwc64x.glsl");
    gl::shader cs_rngGauss(GL_COMPUTE_SHADER, SHADER_PATH "rng/rng_gauss.comp");
    cs_rngGauss.setIncludeSrc("uint64.glsl", SHADER_PATH "lib/uint64.glsl");
    cs_rngGauss.setIncludeSrc("rng_mwc_skip.glsl", SHADER_PATH "rng/rng_mwc_skip.glsl");
    cs_rngGauss.setIncludeSrc("rng_mwc64x.glsl", SHADER_PATH "rng/rng_mwc64x.glsl");
    cs_rngGauss.setIncludeSrc("rng_tables.glsl", SHADER_PATH "rng/rng_tables.glsl");
    gl::shader cs_clusterpos(GL_COMPUTE_SHADER, SHADER_PATH "cluster_gen/clusterpos.comp");
    gl::shader cs_clustercol(GL_COMPUTE_SHADER, SHADER_PATH "cluster_gen/clustercol.comp");

    // Render shaders
    gl::shader vs_renderCluster(GL_VERTEX_SHADER, SHADER_PATH "cluster/cluster.vert");
    vs_renderCluster.setIncludeSrc("math.glsl", SHADER_PATH "lib/math.glsl");
    vs_renderCluster.setIncludeSrc("complex.glsl", SHADER_PATH "geodesic/complex.glsl");
    vs_renderCluster.setIncludeSrc("elliptic.glsl", SHADER_PATH "geodesic/elliptic.glsl");
    vs_renderCluster.setIncludeSrc("geodesic.glsl", SHADER_PATH "geodesic/geodesic.glsl");
    vs_renderCluster.setIncludeSrc("transf.glsl", SHADER_PATH "lib/transf.glsl");
    gl::shader fs_renderCluster(GL_FRAGMENT_SHADER, SHADER_PATH "cluster/cluster.frag");
    gl::shader cs_genBgTex(GL_COMPUTE_SHADER, SHADER_PATH "bg/bg.comp");
    cs_genBgTex.setIncludeSrc("math.glsl", SHADER_PATH "lib/math.glsl");
    cs_genBgTex.setIncludeSrc("complex.glsl", SHADER_PATH "geodesic/complex.glsl");
    cs_genBgTex.setIncludeSrc("elliptic.glsl", SHADER_PATH "geodesic/elliptic.glsl");
    cs_genBgTex.setIncludeSrc("geodesic.glsl", SHADER_PATH "geodesic/geodesic.glsl");
    cs_genBgTex.setIncludeSrc("transf.glsl", SHADER_PATH "lib/transf.glsl");
    gl::shader vs_renderBg(GL_VERTEX_SHADER, SHADER_PATH "bg/bg.vert");
    gl::shader fs_renderBg(GL_FRAGMENT_SHADER, SHADER_PATH "bg/bg.frag");

    // // Test shader
    // gl::shader cs_test(GL_COMPUTE_SHADER, SHADER_PATH "test/test.comp");

    // Build programs
    std::string log;

    // Cluster generation programs
    prog_rngUniform = new gl::program();
    prog_rngUniform->attachShader(cs_rngUniform);
    if (!prog_rngUniform->build(&log)) {
      std::cerr << "Rng uniform program" << std::endl;
      std::cerr << "═══════════════════" << std::endl;
      std::cerr << log << std::endl;
      throw;
    }
    prog_rngGauss = new gl::program();
    prog_rngGauss->attachShader(cs_rngGauss);
    if (!prog_rngGauss->build(&log)) {
      std::cerr << "Rng gauss program" << std::endl;
      std::cerr << "═════════════════" << std::endl;
      std::cerr << log << std::endl;
      throw;
    }

    prog_clusterpos = new gl::program();
    prog_clusterpos->attachShader(cs_clusterpos);
    if (!prog_clusterpos->build(&log)) {
      std::cerr << "Cluster position program" << std::endl;
      std::cerr << "════════════════════════" << std::endl;
      std::cerr << log << std::endl;
      throw;
    }
    prog_clustercol = new gl::program();
    prog_clustercol->attachShader(cs_clustercol);
    if (!prog_clustercol->build(&log)) {
      std::cerr << "Cluster color program" << std::endl;
      std::cerr << "═════════════════════" << std::endl;
      std::cerr << log << std::endl;
      throw;
    }

    // Render programs
    prog_renderCluster = new gl::program();
    prog_renderCluster->attachShader(vs_renderCluster);
    prog_renderCluster->attachShader(fs_renderCluster);
    if (!prog_renderCluster->build(&log)) {
      std::cerr << "Cluster render program" << std::endl;
      std::cerr << "══════════════════════" << std::endl;
      std::cerr << log << std::endl;
      throw;
    }
    prog_genBgTex = new gl::program();
    prog_genBgTex->attachShader(cs_genBgTex);
    if (!prog_genBgTex->build(&log)) {
      std::cerr << "Background texture generation program" << std::endl;
      std::cerr << "═════════════════════════════════════" << std::endl;
      std::cerr << log << std::endl;
      throw;
    }
    prog_renderBg = new gl::program();
    prog_renderBg->attachShader(vs_renderBg);
    prog_renderBg->attachShader(fs_renderBg);
    if (!prog_renderBg->build(&log)) {
      std::cerr << "Background render program" << std::endl;
      std::cerr << "═════════════════════════" << std::endl;
      std::cerr << log << std::endl;
      throw;
    }

    // // Test program
    // prog_test = new gl::program();
    // prog_test->attachShader(cs_test);
    // if (!prog_test->build(&log)) {
    //   std::cerr << "Test program" << std::endl;
    //   std::cerr << log << std::endl;
    //   throw;
    // }

    // Initialize Background rendering Buffers
    glGenVertexArrays(1, &glBgVerts);
    glGenBuffers(1, &glBgPosBuf);
    glGenBuffers(1, &glBgElementBuf);

    glBindVertexArray(glBgVerts);

    std::vector<glm::vec2> pos = {
      { -1.0f, -1.0f },
      { 1.0f, -1.0f },
      { -1.0f, 1.0f },
      { 1.0f, 1.0f }
    };
    glBindBuffer(GL_ARRAY_BUFFER, glBgPosBuf);
    glBufferData(GL_ARRAY_BUFFER, pos.size() * sizeof(glm::vec2), pos.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, false, 0, (void*)0);

    std::vector<uint> elements = {
      0, 2, 1,
      1, 2, 3
    };
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glBgElementBuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements.size() * sizeof(uint), elements.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    // Initialize background textures
    glGenTextures(1, &glBgTex);
    glBindTexture(GL_TEXTURE_2D, glBgTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &glBgRenderTex);
    glBindTexture(GL_TEXTURE_2D, glBgRenderTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    config = CONFIG_INIT;
  }

  void setCamera(glm::vec3 pos, glm::vec3 lookDir, glm::vec3 upDir,
    float fov, float aspect, float zNear, float zFar) {
    assert(isInit());
    setCameraPos(pos);
    setCameraLookDir(lookDir);
    setCameraUpDir(upDir);
    setCameraFov(fov);
    setCameraAspect(aspect);
    setCameraZNear(zNear);
    setCameraZFar(zFar);
  }
  void setCameraView(glm::vec3 pos, glm::vec3 lookDir, glm::vec3 upDir) {
    assert(isInit());
    setCameraPos(pos);
    setCameraLookDir(lookDir);
    setCameraUpDir(upDir);
  }
  void setCameraPos(glm::vec3 pos) {
    assert(isInit());
    cam.pos = pos;
  }
  void setCameraLookDir(glm::vec3 lookDir) {
    assert(isInit());
    cam.lookDir = lookDir;
  }
  void setCameraUpDir(glm::vec3 upDir) {
    assert(isInit());
    cam.upDir = upDir;
  }
  void setCameraFov(float fov) {
    assert(isInit());
    cam.fov = fov;
  }
  void setCameraAspect(float aspect) {
    assert(isInit());
    cam.aspect = aspect;
  }
  void setCameraZNear(float zNear) {
    assert(isInit());
    cam.zNear = zNear;
  }
  void setCameraZFar(float zFar) {
    assert(isInit());
    cam.zFar = zFar;
  }
  void updateCamera() {
    assert(isInit());
    
    P = glm::perspective(cam.fov, cam.aspect, cam.zNear, cam.zFar);
    P_I = glm::mat4(0.0f);
    P_I[0][0] = 1.0f / P[0][0];
    P_I[1][1] = 1.0f / P[1][1];

    if (!hasCamera())
      config = (slConfig)(config | CONFIG_HAS_CAM);
    updateCameraView();
  }
  void updateCameraView() {
    assert(isInit() && hasCamera());

    V = glm::lookAt(cam.pos, cam.pos + cam.lookDir, cam.upDir);
    glm::mat4 PV = P * V;

    glUseProgram(prog_genBgTex->id);
    glUniform4fv(0, 1, &cam.pos[0]);
    glUniformMatrix2fv(1, 1, false, &P_I[0][0]);
    glUniformMatrix4fv(2, 1, true, &V[0][0]);
    glUseProgram(0);

    glUseProgram(prog_renderCluster->id);
    glUniform4fv(0, 1, &cam.pos[0]);
    glUniformMatrix4fv(1, 1, false, &PV[0][0]);
    glUseProgram(0);

    if (!hasCamera())
      config = (slConfig)(config | CONFIG_HAS_CAM);
  }

  void setBackgroundTex(uint width, uint height, std::vector<char>* data) {
    assert(isInit());
    setBackgroundTexSize(width, height);
    setBackgroundTexData(data);
  }
  void setBackgroundTexSize(uint width, uint height) {
    assert(isInit());
    bgTexWidth = width;
    bgTexHeight = height;
  }
  void setBackgroundTexData(std::vector<char>* data) {
    assert(isInit());
    bgTexData = data;
  }
  void updateBackgroundTex() {
    assert(isInit());

    // Set background texture to passed image
    glBindTexture(GL_TEXTURE_2D, glBgTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, bgTexWidth, bgTexHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, bgTexData->data());
    
    glBindTexture(GL_TEXTURE_2D, glBgRenderTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    config = (slConfig)(config | CONFIG_HAS_BG_TEX);
  }
  void updateBackgroundTexData() {
    assert(isInit() && hasBgTex());

    // Set background texture to passed image
    glBindTexture(GL_TEXTURE_2D, glBgTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bgTexWidth, bgTexHeight, GL_RGBA, GL_UNSIGNED_BYTE, bgTexData->data());
  }

  void renderClassic() {
    assert(isInit() && hasCamera() && hasBgTex());

    // Generate texture to render
    glUseProgram(prog_genBgTex->id);
    glUniform2ui(3, bgTexWidth, bgTexHeight);
    glUniform1i(4, 0);
    glBindImageTexture(0, glBgTex, 0, false, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glUniform1i(5, 1);
    glBindImageTexture(1, glBgRenderTex, 0, false, 0, GL_READ_ONLY, GL_RGBA32F);
    glDispatchCompute(1920 / 8, 1080 / 8, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glUseProgram(0);

    glUseProgram(prog_renderBg->id);
    glBindVertexArray(glBgVerts);
    
    // Bind texture
    glBindTexture(GL_TEXTURE_2D, glBgRenderTex);

    // Render Background
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);
    glBindVertexArray(0);
    glUseProgram(0);

    // Render Particles
    glUseProgram(prog_renderCluster->id);
    for (int i = 0; i < (int)glClusterVerts.size(); i++) {
      glBindVertexArray(glClusterVerts[i]);

      glUniform1i(2, 0);
      glUniform1ui(3, 0);
      glDrawArrays(GL_POINTS, 0, (int)(nClusterVerts[i]));

      glUniform1ui(3, 1);
      glDrawArrays(GL_POINTS, 0, (int)(nClusterVerts[i]));

      glUniform1ui(2, 1);
      glUniform1ui(3, 0);
      glDrawArrays(GL_POINTS, 0, (int)(nClusterVerts[i]));

      glUniform1ui(3, 1);
      glDrawArrays(GL_POINTS, 0, (int)(nClusterVerts[i]));

      glBindVertexArray(0);
    }
    glUseProgram(0);

    glFinish();
  }
  void renderRelativistic() {

  }

  void updateParticlebgTexWidthsic(UNUSED float time) {

  }
  void updateParticlebgTexWidthtivistic(UNUSED float time) {
    
  }

  void createEllipticCluster(uint nParticles, float a, float b, glm::vec3 n, float dr, float dz,
    std::vector<glm::vec4> palette, std::vector<float> blurSizes) {
    
    /// TEST
    // #define N 1000
    // float bVals[N];
    // for (int i = 0; i < N; ++i) {
    //   bVals[i] = 0.001f + (10.0f - 0.001f) * float(i) / float(N - 1);
    // }

    // GLuint bBuf;
    // GLuint phi1Buf;
    // GLuint dphi1Buf;
    // GLuint phi2Buf;
    // GLuint dphi2Buf;
    // glGenBuffers(1, &bBuf);
    // glGenBuffers(1, &phi1Buf);
    // glGenBuffers(1, &dphi1Buf);
    // glGenBuffers(1, &phi2Buf);
    // glGenBuffers(1, &dphi2Buf);
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, bBuf);
    // glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(bVals), bVals, GL_STREAM_READ);
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, phi1Buf);
    // glBufferData(GL_SHADER_STORAGE_BUFFER, N * sizeof(glm::vec2), NULL, GL_STREAM_READ);
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, dphi1Buf);
    // glBufferData(GL_SHADER_STORAGE_BUFFER, N * sizeof(glm::vec2), NULL, GL_STREAM_READ);
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, phi2Buf);
    // glBufferData(GL_SHADER_STORAGE_BUFFER, N * sizeof(glm::vec2), NULL, GL_STREAM_READ);
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, dphi2Buf);
    // glBufferData(GL_SHADER_STORAGE_BUFFER, N * sizeof(glm::vec2), NULL, GL_STREAM_READ);

    // glUseProgram(test_prog);
    // glUniform1f(0, 0.0f);
    // glUniform1f(1, 0.25f);
    // glUniform1f(2, 0.0f);
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bBuf);
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, phi1Buf);
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, dphi1Buf);
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, phi2Buf);
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, dphi2Buf);
    // glDispatchCompute(N, 1, 1);
    // glUseProgram(0);

    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, phi1Buf);
    // glm::vec2* phi1 = (glm::vec2*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, N * sizeof(glm::vec2), GL_MAP_READ_BIT);
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, dphi1Buf);
    // glm::vec2* dphi1 = (glm::vec2*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, N * sizeof(glm::vec2), GL_MAP_READ_BIT);
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, phi2Buf);
    // glm::vec2* phi2 = (glm::vec2*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, N * sizeof(glm::vec2), GL_MAP_READ_BIT);
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, dphi2Buf);
    // glm::vec2* dphi2 = (glm::vec2*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, N * sizeof(glm::vec2), GL_MAP_READ_BIT);

    // std::ofstream out1;
    // out1.open("test/samples1.txt");
    // if (!out1.is_open()) throw;
    // std::ofstream out2;
    // out2.open("test/samples2.txt");
    // if (!out2.is_open()) throw;

    // for (int i = 0; i < N; ++i) {
    //   out1 << phi1[i].x << "," << phi1[i].y << ";" << dphi1[i].x << "," << dphi1[i].y << std::endl;
    //   out2 << phi2[i].x << "," << phi2[i].y << ";" << dphi2[i].x << "," << dphi2[i].y << std::endl;
    // }
    // throw;

    // GLuint resBuf;
    // glGenBuffers(1, &resBuf);
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, resBuf);
    // glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec2), NULL, GL_STREAM_READ);

    // glUseProgram(test_prog);
    // glUniform2f(0, float(M_PI_2), 0.4f);
    // glUniform2f(1, 0.5f, 0.0f);
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, resBuf);
    // glDispatchCompute(1, 1, 1);
    // glUseProgram(0);

    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, resBuf);
    // glm::vec2* res = (glm::vec2*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(glm::vec2), GL_MAP_READ_BIT);
    // std::cout << res[0].x << " + " << res[0].y << "j" << std::endl;
    // throw;
    /// TEST
    
    std::cout << "Compute cluster... ";

    glFinish();
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
    
    // Initialize cluster buffers
    GLuint clusterVerts;
    GLuint posBuf;
    GLuint colorBuf;
    glGenVertexArrays(1, &clusterVerts);
    glGenBuffers(1, &posBuf);
    glGenBuffers(1, &colorBuf);
    
    glBindVertexArray(clusterVerts);
    glBindBuffer(GL_ARRAY_BUFFER, posBuf);
    glBufferData(GL_ARRAY_BUFFER, nParticles * sizeof(glm::vec4), NULL, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, false, 0, (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, colorBuf);
    glBufferData(GL_ARRAY_BUFFER, nParticles * sizeof(glm::vec4), NULL, GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, false, 0, (void*)0);
    glBindVertexArray(0);

    // Initialize buffers
    GLuint uSamples1Buf;
    GLuint uSamples2Buf;
    GLuint nSamples1Buf;
    GLuint nSamples2Buf;
    GLuint paletteBuf;
    GLuint blurSizesBuf;
    glGenBuffers(1, &uSamples1Buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, uSamples1Buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, nParticles * sizeof(float), NULL, GL_STREAM_READ);
    glGenBuffers(1, &uSamples2Buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, uSamples2Buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, nParticles * sizeof(float), NULL, GL_STREAM_READ);
    glGenBuffers(1, &nSamples1Buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, nSamples1Buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, nParticles * sizeof(float), NULL, GL_STREAM_READ);
    glGenBuffers(1, &nSamples2Buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, nSamples2Buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, nParticles * sizeof(float), NULL, GL_STREAM_READ);
    glGenBuffers(1, &paletteBuf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, paletteBuf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, palette.size() * sizeof(glm::vec4), palette.data(), GL_STREAM_READ);
    glGenBuffers(1, &blurSizesBuf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, blurSizesBuf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, blurSizes.size() * sizeof(float), blurSizes.data(), GL_STREAM_READ);

    // Generate uniform samples
    uint64 off_ = getRngOff();
    glm::uvec2 off = glm::uvec2((uint)((off_ >> 8 * sizeof(uint)) & UINT_MAX), (uint)(off_ & UINT_MAX));
    glUseProgram(prog_rngUniform->id);
    glUniform1ui(0, nParticles);
    glUniform2ui(1, off.x, off.y);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, uSamples1Buf);
    glDispatchCompute(0x10 / 4, 0x10 / 4, 0x10 / 4);

    off_ = getRngOff();
    off = glm::uvec2((uint)((off_ >> 8 * sizeof(uint)) & UINT_MAX), (uint)(off_ & UINT_MAX));
    glUniform2ui(1, off.x, off.y);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, uSamples2Buf);
    glDispatchCompute(0x10 / 4, 0x10 / 4, 0x10 / 4);
    glUseProgram(0);

    // Generate gaussian samples
    off_ = getRngOff();
    off = glm::uvec2((uint)((off_ >> 8 * sizeof(uint)) & UINT_MAX), (uint)(off_ & UINT_MAX));
    glUseProgram(prog_rngGauss->id);
    glUniform1ui(0, nParticles);
    glUniform2ui(1, off.x, off.y);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, nSamples1Buf);
    glDispatchCompute(0x10 / 4, 0x10 / 4, 0x10 / 4);

    off_ = getRngOff();
    off = glm::uvec2((uint)((off_ >> 8 * sizeof(uint)) & UINT_MAX), (uint)(off_ & UINT_MAX));
    glUniform2ui(1, off.x, off.y);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, nSamples2Buf);
    glDispatchCompute(0x10 / 4, 0x10 / 4, 0x10 / 4);
    glUseProgram(0);

    glFinish();
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();

    float eps = sqrt(1 - b * b / (a * a));
    glm::mat4 rot = glm::rotate(n.z / glm::length(n), glm::vec3(-n.y, n.x, 0.0));
    glUseProgram(prog_clusterpos->id);
    glUniform1ui(0, nParticles);
    glUniform1f(1, b);
    glUniform1f(2, eps);
    glUniformMatrix4fv(3, 1, false, &rot[0][0]);
    glUniform1f(4, dr);
    glUniform1f(5, dz);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, uSamples1Buf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, nSamples1Buf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, nSamples2Buf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, posBuf);
    glDispatchCompute(0x10 / 4, 0x10 / 4, 0x10 / 4);
    glUseProgram(0);

    glUseProgram(prog_clustercol->id);
    glUniform1ui(0, nParticles);
    glUniform1ui(1, palette.size());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, paletteBuf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, blurSizesBuf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, uSamples2Buf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, colorBuf);
    glDispatchCompute(0x10 / 4, 0x10 / 4, 0x10 / 4);
    glUseProgram(0);

    nClusterVerts.push_back(nParticles);
    glClusterVerts.push_back(clusterVerts);
    glClusterPosBufs.push_back(posBuf);
    glClusterColBufs.push_back(colorBuf);

    glFinish();
    std::chrono::high_resolution_clock::time_point t3 = std::chrono::high_resolution_clock::now();
    float dt1 = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count() * 1.0e-9;
    float dt2 = std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t2).count() * 1.0e-9;
    float dt = std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t1).count() * 1.0e-9;
    std::cout << "Done" << std::endl;
    std::cout << "Sample computation time for " << nParticles << " particles: " << dt1 << " s" << std::endl;
    std::cout << "Cluster computation time for " << nParticles << " particles: " << dt2 << " s" << std::endl;
    std::cout << "Total computation time for " << nParticles << " particles: " << dt << " s" << std::endl;
    std::cout << std::endl;
  }
  void clearClusters() {
    // Clear particle GPU buffers
    for (int i = 0; i < (int)glClusterVerts.size(); i++) {
      glDeleteBuffers(1, &glClusterPosBufs[i]);
      glDeleteBuffers(1, &glClusterColBufs[i]);
      glDeleteVertexArrays(1, &glClusterVerts[i]);
    }
    nClusterVerts.clear();
    glClusterVerts.clear();
    glClusterPosBufs.clear();
    glClusterColBufs.clear();
  }
  void selectCluster(int index) {
    sCluster = index;
  }
  void deselectClusters() {
    sCluster = -1;
  }

  void close() {
    assert((config & CONFIG_INIT) != 0);

    clearClusters();

    glDeleteTextures(1, &glBgTex);
    glDeleteTextures(1, &glBgRenderTex);

    glDeleteBuffers(1, &glBgPosBuf);
    glDeleteBuffers(1, &glBgElementBuf);
    glDeleteVertexArrays(1, &glBgVerts);

    delete prog_rngUniform;
    delete prog_rngGauss;
    delete prog_clusterpos;
    delete prog_clustercol;
    delete prog_renderCluster;
    delete prog_renderBg;

    config = CONFIG_CLOSED;
  }

  uint64 getRngOff() {
    uint64 dt = time(0);
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    t1 = std::chrono::floor<std::chrono::seconds>(t1);
    return dt * 1000000000ULL + (uint64)std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
  }

  void GLAPIENTRY msgCallback(GLenum source, GLenum type, UNUSED GLuint id, GLenum severity,
    UNUSED GLsizei length, const GLchar* message, UNUSED const void* userParam) {
    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION) {
      fprintf(stderr, "%s\t %s, type: %s, source: %s\n",
        getErrSeverity(severity), message, getErrType(type), getErrSource(source));
    }
  }
  const char* getErrSource(GLenum source) {
    switch (source)
    {
    case GL_DEBUG_SOURCE_API: return "API";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "Window system";
    case GL_DEBUG_SOURCE_SHADER_COMPILER: return "Shader compiler";
    case GL_DEBUG_SOURCE_THIRD_PARTY: return "Third party";
    case GL_DEBUG_SOURCE_APPLICATION: return "Application";
    case GL_DEBUG_SOURCE_OTHER: return "Other";
    default: return "Unknown error source";
    }
  }
  const char* getErrType(GLenum type) {
    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR: return "Error";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "Deprecated behavior";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "Undefined behavior";
    case GL_DEBUG_TYPE_PORTABILITY: return "Portability";
    case GL_DEBUG_TYPE_PERFORMANCE: return "Performance";
    case GL_DEBUG_TYPE_MARKER: return "Marker";
    case GL_DEBUG_TYPE_PUSH_GROUP: return "Push group";
    case GL_DEBUG_TYPE_POP_GROUP: return "Pop Group";
    case GL_DEBUG_TYPE_OTHER: return "Other";
    default: return "Unknown error type";
    }
  }
  const char* getErrSeverity(GLenum severity) {
    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH: return "Error";
    case GL_DEBUG_SEVERITY_MEDIUM: return "Major warning";
    case GL_DEBUG_SEVERITY_LOW: return "Warning";
    case GL_DEBUG_SEVERITY_NOTIFICATION: return "Note";
    default: return "Unknown error severity";
    }
  }
}