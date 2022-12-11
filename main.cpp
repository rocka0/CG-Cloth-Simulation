// clang-format off
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GL/wglew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
// clang-format on

using namespace std;

constexpr int WIDTH = 1440;    // Width of the window
constexpr int HEIGHT = 900;    // Height of the window

constexpr int N = 9;                            // Number of points per row/col of cloth grid: [0, N]
constexpr int pointCount = (N + 1) * (N + 1);    // Total number of points in the grid

constexpr float fullsize = 8.0;
constexpr float halfsize = fullsize / 2;

constexpr int GRID_SIZE = 8;

constexpr float timeStep = 1 / 30.0f;
float currentTime = 0;
double accumulator = timeStep;

int mouseSelectedIndex = -1;

int oldX = 0, oldY = 0;
float rX = 15, rY = 0;
int state = 1;
float dist = -23;

constexpr float DEFAULT_DAMPING = -0.0125f;
constexpr float KsStruct = 0.5f, KdStruct = -0.25f;
constexpr float KsShear = 0.5f, KdShear = -0.25f;
constexpr float KsBend = 0.85f, KdBend = -0.25f;
glm::vec3 gravity = glm::vec3(0.0f, -0.00981f, 0.0f);
constexpr float mass = 0.5f;

GLint viewport[4];
GLdouble MV[16];
GLdouble P[16];

glm::vec3 Up = glm::vec3(0, 1, 0), Right, viewDir;

LARGE_INTEGER frequency, t1, t2;
double frameTimeQP = 0;
float frameTime = 0;

float startTime = 0, fps = 0;
int totalFrames = 0;

vector<GLushort> triangleIndices;

struct Point {
    glm::vec3 pos;
    glm::vec3 prevPos;
    glm::vec3 velocity;
    glm::vec3 force;

    void print() {
        printf("Point: (%f, %f, %f)\n", pos.x, pos.y, pos.z);
    }
};

vector<Point> points;

enum SPRING_TYPE { STRUCTURAL, SHEAR, BEND };

struct Spring {
    int p1;
    int p2;
    float restLength;
    float Ks;
    float Kd;
    SPRING_TYPE type;

    Spring(int p1, int p2, float Ks, float Kd, SPRING_TYPE type) {
        this->p1 = p1;
        this->p2 = p2;
        this->Ks = Ks;
        this->Kd = Kd;
        this->type = type;
        glm::vec3 diff = points[p1].pos - points[p2].pos;
        this->restLength = sqrt(glm::dot(diff, diff));
    }
};

vector<Spring> springs;

void AddSpring(int a, int b, float Ks, float Kd, SPRING_TYPE type) {
    Spring spring(a, b, Ks, Kd, type);
    springs.push_back(spring);
}

void InitGL() {
    startTime = (float) glutGet(GLUT_ELAPSED_TIME);
    currentTime = startTime;
    glEnable(GL_DEPTH_TEST);
    int count = 0;
    int n = N + 1;

    triangleIndices.resize(N * N * 2 * 3);
    points.resize(pointCount);

    // fill in X
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < n; i++) {
            Point &p = points[count++];
            p.pos = glm::vec3(((float(i) / (n - 1)) * 2 - 1) * halfsize, fullsize + 1, ((float(j) / (n - 1)) * fullsize));
            p.velocity = glm::vec3(0);
        }
    }

    // fill in triangleIndices
    GLushort *id = &triangleIndices[0];
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            int i0 = i * (N + 1) + j;
            int i1 = i0 + 1;
            int i2 = i0 + (N + 1);
            int i3 = i2 + 1;
            if ((j + i) % 2) {
                *id++ = i0;
                *id++ = i2;
                *id++ = i1;
                *id++ = i1;
                *id++ = i2;
                *id++ = i3;
            } else {
                *id++ = i0;
                *id++ = i2;
                *id++ = i3;
                *id++ = i0;
                *id++ = i3;
                *id++ = i1;
            }
        }
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glPointSize(5);

    wglSwapIntervalEXT(0);

    // setup springs
    // Horizontal
    for (int l1 = 0; l1 < n; l1++)    // v
        for (int l2 = 0; l2 < (n - 1); l2++) {
            AddSpring((l1 * n) + l2, (l1 * n) + l2 + 1, KsStruct, KdStruct, STRUCTURAL);
        }

    // Vertical
    for (int l1 = 0; l1 < n; l1++)
        for (int l2 = 0; l2 < (n - 1); l2++) {
            AddSpring((l2 * n) + l1, ((l2 + 1) * n) + l1, KsStruct, KdStruct, STRUCTURAL);
        }

    // Shearing Springs
    for (int l1 = 0; l1 < (n - 1); l1++) {
        for (int l2 = 0; l2 < (n - 1); l2++) {
            AddSpring((l1 * n) + l2, ((l1 + 1) * n) + l2 + 1, KsShear, KdShear, SHEAR);
            AddSpring(((l1 + 1) * n) + l2, (l1 * n) + l2 + 1, KsShear, KdShear, SHEAR);
        }
    }

    // Bend Springs
    for (int l1 = 0; l1 < n; l1++) {
        for (int l2 = 0; l2 < (n - 2); l2++) {
            AddSpring((l1 * n) + l2, (l1 * n) + l2 + 2, KsBend, KdBend, BEND);
        }
        AddSpring((l1 * n) + (n - 3), (l1 * n) + (n - 1), KsBend, KdBend, BEND);
    }
    for (int l1 = 0; l1 < n; l1++) {
        for (int l2 = 0; l2 < (n - 2); l2++) {
            AddSpring((l2 * n) + l1, ((l2 + 2) * n) + l1, KsBend, KdBend, BEND);
        }
        AddSpring(((n - 3) * n) + l1, ((n - 1) * n) + l1, KsBend, KdBend, BEND);
    }
}

void DrawGrid() {
    glBegin(GL_LINES);
    glColor3f(1.0f, 1.0f, 1.0f);
    for (int i = -GRID_SIZE; i <= GRID_SIZE; i++) {
        glVertex3f((float) i, 0, (float) -GRID_SIZE);
        glVertex3f((float) i, 0, (float) GRID_SIZE);
        glVertex3f((float) -GRID_SIZE, 0, (float) i);
        glVertex3f((float) GRID_SIZE, 0, (float) i);
    }
    glEnd();
}

void OnRender() {
    size_t i = 0;
    float newTime = (float) glutGet(GLUT_ELAPSED_TIME);
    frameTime = newTime - currentTime;
    currentTime = newTime;
    QueryPerformanceCounter(&t2);
    frameTimeQP = (t2.QuadPart - t1.QuadPart) * 1000.0 / frequency.QuadPart;
    t1 = t2;
    accumulator += frameTimeQP;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0, 0, dist);
    glRotatef(rX, 1, 0, 0);
    glRotatef(rY, 0, 1, 0);

    glGetDoublev(GL_MODELVIEW_MATRIX, MV);
    viewDir.x = (float) -MV[2];
    viewDir.y = (float) -MV[6];
    viewDir.z = (float) -MV[10];
    Right = glm::cross(viewDir, Up);

    // draw grid
    DrawGrid();

    // draw polygons
    glColor3f(1, 1, 1);
    glBegin(GL_TRIANGLES);
    for (i = 0; i < triangleIndices.size(); i += 3) {
        auto &P1 = points[triangleIndices[i]].pos;
        auto &P2 = points[triangleIndices[i + 1]].pos;
        auto &P3 = points[triangleIndices[i + 2]].pos;
        glVertex3f(P1.x, P1.y, P1.z);
        glVertex3f(P2.x, P2.y, P2.z);
        glVertex3f(P3.x, P3.y, P3.z);
    }
    glEnd();

    // draw points
    glBegin(GL_POINTS);
    for (i = 0; i < pointCount; i++) {
        auto &P = points[i].pos;
        int is = (i == mouseSelectedIndex);
        glColor3f((float) !is, (float) is, (float) is);
        glVertex3f(P.x, P.y, P.z);
    }
    glEnd();

    glutSwapBuffers();
}

void ComputeForces() {
    for (int i = 0; i < pointCount; i++) {
        auto &V = points[i].velocity;
        auto &F = points[i].force;
        F = glm::vec3(0);

        // add gravity force only for non-fixed points
        if (i != 0 && i != N) F += gravity;

        // add force due to damping of velocity
        F += DEFAULT_DAMPING * V;
    }

    // add spring forces
    for (size_t i = 0; i < springs.size(); i++) {
        auto &P1 = points[springs[i].p1].pos;
        auto &V1 = points[springs[i].p1].velocity;
        auto &F1 = points[springs[i].p1].force;
        auto &P2 = points[springs[i].p2].pos;
        auto &V2 = points[springs[i].p2].velocity;
        auto &F2 = points[springs[i].p2].force;

        glm::vec3 deltaP = P1 - P2;
        glm::vec3 deltaV = V1 - V2;
        float dist = glm::length(deltaP);

        float leftTerm = -springs[i].Ks * (dist - springs[i].restLength);
        float rightTerm = springs[i].Kd * (glm::dot(deltaV, deltaP) / dist);
        glm::vec3 springForce = (leftTerm + rightTerm) * glm::normalize(deltaP);

        if (springs[i].p1 != 0 && springs[i].p1 != N) F1 += springForce;
        if (springs[i].p2 != 0 && springs[i].p2 != N) F2 -= springForce;
    }
}

void IntegrateEuler(float deltaTime) {
    float deltaTimeMass = deltaTime / mass;
    for (int i = 0; i < pointCount; i++) {
        auto &P = points[i].pos;
        auto &V = points[i].velocity;
        auto &F = points[i].force;
        glm::vec3 oldV = V;
        V += (F * deltaTimeMass);
        P += deltaTime * oldV;
        if (P.y < 0) {
            P.y = 0;
        }
    }
}

void ApplyProvotDynamicInverse() {
    for (size_t i = 0; i < springs.size(); i++) {
        auto &P1 = points[springs[i].p1].pos;
        auto &V1 = points[springs[i].p1].velocity;
        auto &P2 = points[springs[i].p2].pos;
        auto &V2 = points[springs[i].p2].velocity;
        glm::vec3 deltaP = P1 - P2;
        float dist = glm::length(deltaP);
        if (dist > springs[i].restLength) {
            dist -= (springs[i].restLength);
            dist /= 2.0f;
            deltaP = glm::normalize(deltaP);
            deltaP *= dist;
            if (springs[i].p1 == 0 || springs[i].p1 == N) {
                V2 += deltaP;
            } else if (springs[i].p2 == 0 || springs[i].p2 == N) {
                V1 -= deltaP;
            } else {
                V1 -= deltaP;
                V2 += deltaP;
            }
        }
    }
}

void StepPhysics(float deltaTime) {
    ComputeForces();
    IntegrateEuler(deltaTime);
    ApplyProvotDynamicInverse();
}

void OnIdle() {
    if (accumulator >= timeStep) {
        StepPhysics(timeStep);
        accumulator -= timeStep;
    }
    glutPostRedisplay();
}

void OnReshape(int nw, int nh) {
    glViewport(0, 0, nw, nh);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60, (GLfloat) nw / (GLfloat) nh, 1.f, 100.0f);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_PROJECTION_MATRIX, P);
    glMatrixMode(GL_MODELVIEW);
}

void OnMouseDown(int button, int s, int x, int y) {
    if (s == GLUT_DOWN) {
        oldX = x;
        oldY = y;
        int window_y = (HEIGHT - y);
        float norm_y = float(window_y) / float(HEIGHT / 2.0);
        int window_x = x;
        float norm_x = float(x) / float(WIDTH / 2.0);
        float winZ = 0;
        glReadPixels(x, HEIGHT - y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &winZ);
        if (winZ == 1) winZ = 0;
        double objX = 0, objY = 0, objZ = 0;
        gluUnProject(window_x, window_y, winZ, MV, P, viewport, &objX, &objY, &objZ);
        glm::vec3 pt(objX, objY, objZ);
        for (int i = 0; i < pointCount; i++) {
            auto &P = points[i].pos;
            if (glm::distance(P, pt) < 0.2) {
                mouseSelectedIndex = i;
                printf("Intersected at %d\t", i);
                points[i].print();
                break;
            }
        }
    }

    if (button == GLUT_RIGHT_BUTTON)
        state = 0;
    else
        state = 1;

    if (s == GLUT_UP) {
        mouseSelectedIndex = -1;
        glutSetCursor(GLUT_CURSOR_INHERIT);
    }
}

void OnMouseMove(int x, int y) {
    if (mouseSelectedIndex == -1) {
        if (state == 0)
            dist *= (1 + (y - oldY) / 60.0f);
        else {
            rY += (x - oldX) / 5.0f;
            rX += (y - oldY) / 5.0f;
        }
    } else {
        float delta = 1500 / abs(dist);
        float valX = (x - oldX) / delta;
        float valY = (oldY - y) / delta;
        if (abs(valX) > abs(valY))
            glutSetCursor(GLUT_CURSOR_LEFT_RIGHT);
        else
            glutSetCursor(GLUT_CURSOR_UP_DOWN);
        auto &P = points[mouseSelectedIndex].pos;
        auto &V = points[mouseSelectedIndex].velocity;
        V = glm::vec3(0);
        P.x += Right[0] * valX;
        float newValue = P.y + Up[1] * valY;
        if (newValue > 0) P.y = newValue;
        P.z += Right[2] * valX + Up[2] * valY;
    }
    oldX = x;
    oldY = y;
    glutPostRedisplay();
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Cloth Simulation");

    glutDisplayFunc(OnRender);
    glutReshapeFunc(OnReshape);
    glutIdleFunc(OnIdle);

    glutMouseFunc(OnMouseDown);
    glutMotionFunc(OnMouseMove);

    glewInit();
    InitGL();

    glutMainLoop();
}
