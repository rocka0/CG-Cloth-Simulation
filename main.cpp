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

constexpr int N = 19;                            // Number of points per row/col of cloth grid: [0, N]
constexpr int pointCount = (N + 1) * (N + 1);    // Total number of points in the grid

constexpr float fullsize = 6.0;    // intial zoom in on cloth
constexpr float halfsize = fullsize / 2;

constexpr float mass = 0.2f;                   // mass of each cloth point
glm::vec3 gravity(0.0f, -0.00981f, 0.0f);      // gravity force vector in International System of Units
constexpr float DEFAULT_DAMPING = -0.0125f;    // velocity damping of point
/*
    Ks = Spring constant in Hooke's law: F = (-Ks)(x)
    Kd = Spring damping constant: F = (-Kd)(v)
    {A damping force is required for realistic numerical simulation to ensure the springs do not oscillate forever}
*/
constexpr float KsStruct = 0.5f, KdStruct = -0.25f;
constexpr float KsShear = 0.5f, KdShear = -0.25f;
constexpr float KsBend = 0.85f, KdBend = -0.25f;

constexpr int GRID_SIZE = 8;    // reference grid size

constexpr float timeStep = 1 / 60.0f;    // dt time interval to update particle parameters
double accumulator = timeStep;           // collects time intervals as frames are rendered
LARGE_INTEGER frequency, t1, t2;         // t1, t2 store high accuracy time between 2 frames: t1 < t2

bool showPoints = true;               // flag which marks if point of cloth are shown or not
bool showStructuralSprings = true;    // flag to show structuralSprings
bool showShearSprings = false;        // flag to show shearSprings
bool showBendSprings = false;         // flag to show bendSprings
int mouseSelectedIndex = -1;          // if != -1, stores index of mouse selected point

int prevX = 0;    // TODO
int prevY = 0;    // TODO

float rX = 45;    // stores the rotation angle along X for panning
float rY = 0;     // stores the rotation angle along Y for panning

bool state = 1;      // tells us if we are zooming or panning
float dist = -30;    // stores distance to scene

GLint viewport[4];        // TODO
GLdouble MV[16];          // TODO
GLdouble P[16];           // TODO
glm::vec3 Up(0, 1, 0);    // TODO
glm::vec3 Right;          // TODO
glm::vec3 viewDir;        // TODO

/*
    TODO
*/
struct Point {
    glm::vec3 pos;
    glm::vec3 velocity;
    glm::vec3 force;
};

vector<Point> points;    // Array that stores all the points of the cloth

enum SPRING_TYPE { STRUCTURAL, SHEAR, BEND };    // Enum to store the various types of springs in the cloth

/*
    TODO
*/
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

vector<Spring> springs;    // Array that stores all the springs in the cloth

/*
    TODO: Setup routine.
*/
void initGL() {
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&t1);
    glEnable(GL_DEPTH_TEST);

    int n = N + 1;

    points.resize(pointCount);
    for (int j = 0, count = 0; j < n; j++) {
        for (int i = 0; i < n; i++) {
            Point &p = points[count++];
            p.pos = glm::vec3(((float(i) / (n - 1)) * 2 - 1) * halfsize, fullsize + 1, ((float(j) / (n - 1)) * fullsize));
            p.velocity = glm::vec3(0);
        }
    }

    // setup springs
    // Horizontal
    for (int i = 0; i < n; i++)
        for (int j = 0; j < (n - 1); j++) {
            springs.push_back(Spring((i * n) + j, (i * n) + j + 1, KsStruct, KdStruct, STRUCTURAL));
        }

    // Vertical
    for (int i = 0; i < n; i++)
        for (int j = 0; j < (n - 1); j++) {
            springs.push_back(Spring((j * n) + i, ((j + 1) * n) + i, KsStruct, KdStruct, STRUCTURAL));
        }

    // Shearing Springs
    for (int i = 0; i < (n - 1); i++) {
        for (int j = 0; j < (n - 1); j++) {
            springs.push_back(Spring((i * n) + j, ((i + 1) * n) + j + 1, KsShear, KdShear, SHEAR));
            springs.push_back(Spring(((i + 1) * n) + j, (i * n) + j + 1, KsShear, KdShear, SHEAR));
        }
    }

    // Bend Springs
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < (n - 2); j++) {
            springs.push_back(Spring((i * n) + j, (i * n) + j + 2, KsBend, KdBend, BEND));
        }
        springs.push_back(Spring((i * n) + (n - 3), (i * n) + (n - 1), KsBend, KdBend, BEND));
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < (n - 2); j++) {
            springs.push_back(Spring((j * n) + i, ((j + 2) * n) + i, KsBend, KdBend, BEND));
        }
        springs.push_back(Spring(((n - 3) * n) + i, ((n - 1) * n) + i, KsBend, KdBend, BEND));
    }

    glPointSize(4);
    wglSwapIntervalEXT(0);
}

/*
    TODO: Drawing Routine.
*/
void drawScene() {
    QueryPerformanceCounter(&t2);
    double frameTimeQP = (t2.QuadPart - t1.QuadPart) * 1000.0 / frequency.QuadPart;
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
    glBegin(GL_LINES);
    glColor3f(1.0f, 1.0f, 1.0f);
    for (int i = -GRID_SIZE; i <= GRID_SIZE; i++) {
        glVertex3f((float) i, 0, (float) -GRID_SIZE);
        glVertex3f((float) i, 0, (float) GRID_SIZE);
        glVertex3f((float) -GRID_SIZE, 0, (float) i);
        glVertex3f((float) GRID_SIZE, 0, (float) i);
    }
    glEnd();

    // draw points
    if (showPoints) {
        glBegin(GL_POINTS);
        for (int i = 0; (int) i < pointCount; i++) {
            auto &P = points[i].pos;
            if (i == mouseSelectedIndex) {
                glColor3f(1.0f, 1.0f, 1.0f);
            } else {
                glColor3f(1.0f, 0.0f, 0.0f);
            }
            glVertex3f(P.x, P.y, P.z);
        }
        glEnd();
    }

    // draw springs
    glBegin(GL_LINES);
    for (auto &s : springs) {
        auto &p1 = points[s.p1].pos;
        auto &p2 = points[s.p2].pos;
        if (s.type == STRUCTURAL and showStructuralSprings) {
            glColor3f(0.0f, 1.0f, 0.0f);
            glVertex3f(p1.x, p1.y, p1.z);
            glVertex3f(p2.x, p2.y, p2.z);
        }
        if (s.type == SHEAR and showShearSprings) {
            glColor3f(248 / 255.0f, 249 / 255.0f, 136 / 255.0f);
            glVertex3f(p1.x, p1.y, p1.z);
            glVertex3f(p2.x, p2.y, p2.z);
        }
        if (s.type == BEND and showBendSprings) {
            glColor3f(107 / 255.0f, 102 / 255.0f, 157 / 255.0f);
            glVertex3f(p1.x, p1.y, p1.z);
            glVertex3f(p2.x, p2.y, p2.z);
        }
    }
    glEnd();

    glutSwapBuffers();
}

/*
    TODO
*/
void computeForces() {
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

/*
    TODO
*/
void eulerIntegrate() {
    for (auto &p : points) {
        auto &P = p.pos;
        auto &V = p.velocity;
        auto &F = p.force;
        P += V * timeStep;
        V += (F * timeStep) / mass;
        P.y = max(P.y, 0.0f);
    }
}

/*
    TODO
*/
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

/*
    TODO:
*/
void OnIdle() {
    if (accumulator >= timeStep) {
        accumulator -= timeStep;
        computeForces();
        eulerIntegrate();
        ApplyProvotDynamicInverse();
    }
    glutPostRedisplay();
}

/*
    TODO: OpenGL window reshape routine.
*/
void resize(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60, (GLfloat) w / (GLfloat) h, 1.0f, 100.0f);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_PROJECTION_MATRIX, P);
    glMatrixMode(GL_MODELVIEW);
}

/*
    TODO: Mouse Click Handler
*/
void mouseDownHandler(int button, int s, int x, int y) {
    if (s == GLUT_DOWN) {
        prevX = x;
        prevY = y;
        int window_y = (HEIGHT - y);
        float norm_y = float((HEIGHT - y)) / float(HEIGHT / 2.0);
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
                printf("Intersected at point: (%f, %f, %f)\n", P.x, P.y, P.z);
                break;
            }
        }
        if (button == GLUT_RIGHT_BUTTON)
            state = 0;
        else
            state = 1;
    } else if (s == GLUT_UP) {
        mouseSelectedIndex = -1;
        glutSetCursor(GLUT_CURSOR_INHERIT);
    }
}

/*
    TODO: Mouse Drag Handler
*/
void mouseMoveHandler(int x, int y) {
    if (mouseSelectedIndex == -1) {
        if (state == 0)
            dist *= (1 + (y - prevY) / 60.0f);
        else {
            rY += (x - prevX) / 5.0f;
            rX += (y - prevY) / 5.0f;
        }
    } else {
        float delta = 1500 / abs(dist);
        float valX = (x - prevX) / delta;
        float valY = (prevY - y) / delta;
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
    prevX = x;
    prevY = y;
    glutPostRedisplay();
}

/*
    TODO: keyDownHandler handler
*/
void keyDownHandler(unsigned char key, int x, int y) {
    switch (key) {
        case 'p': {
            showPoints ^= 1;
            break;
        }
        case 's': {
            showStructuralSprings ^= 1;
            break;
        }
        case 'h': {
            showShearSprings ^= 1;
            break;
        }
        case 'b': {
            showBendSprings ^= 1;
            break;
        }
        default: {
            break;
        }
    }
}

/*
    TODO
*/
void printInfo() {
    printf("Press p to show/hide cloth points.\n");
    printf("Press s to show/hide STRUCTURAL springs.\n");
    printf("Press h to show/hide SHEAR springs.\n");
    printf("Press b to show/hide BEND springs.\n");
}

int main(int argc, char **argv) {
    printInfo();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Cloth Simulation");

    glutDisplayFunc(drawScene);
    glutReshapeFunc(resize);
    glutIdleFunc(OnIdle);

    glutKeyboardFunc(keyDownHandler);
    glutMouseFunc(mouseDownHandler);
    glutMotionFunc(mouseMoveHandler);

    glewInit();
    initGL();

    glutMainLoop();
}
