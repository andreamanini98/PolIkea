#include <cmath>
#include <iostream>

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

void GameLogic(float Ar, glm::mat4 &ViewPrj, glm::mat4 &World, glm::vec3 m, glm::vec3 r, float deltaT) {
    // Parameters
    const float nearPlane = 0.1f;
    const float farPlane = 50.f;

    // Camera target height and distance
    const glm::vec3 upVector(0.0, 1.0, 0.0);
    const float camHeight = 1.0;
    const float camDist = 2.5;
    // Camera Pitch limits
    const float minPitch = glm::radians(-8.75f);
    const float maxPitch = glm::radians(60.0f);
    // Rotation and motion speed
    const float ROT_SPEED = glm::radians(120.0f);
    const float MOVE_SPEED = 4.0f;

    static float cameraYaw = glm::radians(0.0f);
    static float cameraPitch = glm::radians(0.0f);
    static float cameraRoll = glm::radians(0.0f);

    glm::vec3 uxx =
            glm::vec3(
                    glm::rotate(glm::mat4(1), -cameraYaw, glm::vec3(0, 1, 0))
                    *
                    glm::vec4(1,0,0,1)
            );
    glm::vec3 uyy = glm::vec3(0, 1, 0);
    glm::vec3 uzz =
            glm::vec3(
                    glm::rotate(glm::mat4(1), -cameraYaw, glm::vec3(0, 1, 0))
                    *
                    glm::vec4(0,0,-1,1)
            );

    cameraYaw += ROT_SPEED * r.y * deltaT;

    cameraPitch -= ROT_SPEED * r.x * deltaT;
    cameraPitch = cameraPitch < minPitch ? minPitch : (cameraPitch > maxPitch ? maxPitch : cameraPitch);

    cameraRoll += ROT_SPEED * deltaT * r.z;
    cameraRoll = cameraRoll < glm::radians(-180.0f) ? glm::radians(-180.0f) : (cameraRoll > glm::radians( 180.0f) ? glm::radians( 180.0f) : cameraRoll);

    static float charAngle = glm::radians(0.0f);
    float newAngle = charAngle;
    if (std::abs(m.x) > 0 || std::abs(m.z) > 0) {
        newAngle = normalizeAngle(cameraYaw + std::atan2(m.x, m.z));
    }

    int dir;
    auto diff = shortestAngularDistance(charAngle, newAngle, dir);
    charAngle += dir * std::min(deltaT * ROT_SPEED * 4, diff);

    static glm::vec3 CharacterPos = glm::vec3(2.0, 0.0, 3.45706);

    CharacterPos += deltaT * uxx * m.x * MOVE_SPEED;
    CharacterPos += deltaT * uyy * m.y * MOVE_SPEED;
    CharacterPos += deltaT * uzz * m.z * MOVE_SPEED;

    static glm::vec3 PosOld = CharacterPos;
    glm::vec3 P = PosOld * std::exp(-10 * deltaT) + CharacterPos * (1 - std::exp(-10 * deltaT));
    PosOld = P;

    //calculate look at matrix ( same as glm::lookAt )
    World = glm::translate(glm::mat4(1.0), P) * glm::rotate(glm::mat4(1.0), -charAngle, glm::vec3(0, 1, 0));

    glm::vec3 c =
            glm::translate(glm::mat4(1.0f), P) *
            glm::rotate(glm::mat4(1), -cameraYaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::rotate(glm::mat4(1), -cameraPitch, glm::vec3(1.0f, 0.0f, 0.0f)) *
            glm::vec4(0, camHeight, camDist, 1);

    glm::vec3 a = glm::vec3(World * glm::vec4(0, 0, 0, 1)) + glm::vec3(0, camHeight, 0);
    glm::mat4 Mv = glm::lookAt(c, a, upVector); //lookAt matrix

    glm::mat4 perspectiveProjection = glm::perspective(glm::radians(60.0f), Ar, nearPlane, farPlane);
    perspectiveProjection[1][1] *= -1;

    ViewPrj = perspectiveProjection * glm::rotate(glm::mat4(1.0), cameraRoll, glm::vec3(0, 0, 1)) * Mv;
}
