#include <cmath>
#include <iostream>

// Struct containing the parameters to be passed from VTemplate.cpp
struct LookAtStuff {
    float deltaT;
    float cameraYaw;
    float cameraPitch;
    float cameraRoll;
};

inline float normalizeAngle(float angle) {
    // Normalize the angle to be between 0 and 2*pi (0 and 360 degrees)
    angle = fmod(angle, 2 * M_PI);
    if (angle < 0)
        angle += 2 * M_PI;
    return angle;
}

inline float shortestAngularDistance(float src_angle, float dst_angle, int &direction) {
    src_angle = normalizeAngle(src_angle);
    dst_angle = normalizeAngle(dst_angle);

    float diff = std::abs(dst_angle - src_angle);
    direction = (src_angle <= dst_angle) ? 1 : -1;

    // Check if decrementing would be shorter
    if (2 * M_PI - diff < diff) {
        direction *= -1;
        return 2 * M_PI - diff;
    }

    return diff;
}

void
getLookAt(float Ar, glm::mat4 &ViewPrj, glm::mat4 &World, LookAtStuff lookAtStuff, glm::vec3 characterPos,
          float characterAngle) {
    // Parameters
    const float nearPlane = 0.1f;
    const float farPlane = 50.f;
    // Camera target height and distance
    const glm::vec3 upVector(0.0, 1.0, 0.0);
    const float camHeight = 1.0;
    const float camDist = 2.5;

    // Used for damping
    static glm::vec3 PosOld = characterPos;
    glm::vec3 P = PosOld * std::exp(-10 * lookAtStuff.deltaT) + characterPos * (1 - std::exp(-10 * lookAtStuff.deltaT));
    PosOld = P;

    //calculate look at matrix ( same as glm::lookAt )
    World = glm::translate(glm::mat4(1.0), P) * glm::rotate(glm::mat4(1.0), -characterAngle, glm::vec3(0, 1, 0));

    glm::vec3 c =
            glm::translate(glm::mat4(1.0f), P) *
            glm::rotate(glm::mat4(1), -lookAtStuff.cameraYaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::rotate(glm::mat4(1), -lookAtStuff.cameraPitch, glm::vec3(1.0f, 0.0f, 0.0f)) *
            glm::vec4(0, camHeight, camDist, 1);

    glm::vec3 a = glm::vec3(World * glm::vec4(0, 0, 0, 1)) + glm::vec3(0, camHeight, 0);
    glm::mat4 Mv = glm::lookAt(c, a, upVector); //lookAt matrix

    glm::mat4 perspectiveProjection = glm::perspective(glm::radians(60.0f), Ar, nearPlane, farPlane);
    perspectiveProjection[1][1] *= -1;

    ViewPrj = perspectiveProjection * glm::rotate(glm::mat4(1.0), lookAtStuff.cameraRoll, glm::vec3(0, 0, 1)) * Mv;
}
