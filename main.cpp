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

constexpr int WIDTH = 1440;                      ///< Width of the window
constexpr int HEIGHT = 900;                      ///< Height of the window
constexpr int N = 19;                            ///< Number of points per row/col of cloth mesh: [0, N]
constexpr int pointCount = (N + 1) * (N + 1);    ///< Total number of points in the grid
constexpr float pointSpacing = 0.25f;            ///< Gap between two consecutive points of row/col in cloth mesh
constexpr int fixedPointOne = N * (N + 1);       ///< Stationary point
constexpr int fixedPointTwo = pointCount - 1;    ///< Stationary point

constexpr float mass = 0.1f;                     ///< Mass of each cloth point
glm::vec3 gravity(0.0f, -0.00981f, 0.0f);        ///< Gravity force vector in International System of Units
constexpr float DEFAULT_DAMPING = -0.0125f;      ///< Velocity damping of point
constexpr float KsStruct = 0.5f, KdStruct = -0.25f;    ///< Constants for structural spring
constexpr float KsShear = 0.5f, KdShear = -0.25f;      ///< Constant for shear spring
constexpr float KsBend = 0.85f, KdBend = -0.25f;       ///< Constant for bend spring

constexpr float timeStep = 1 / 240.0f;    ///< dt time interval to update particle parameters
double accumulator = 0.0f;                ///< Stores sum of time intervals until next dt time has elapsed
LARGE_INTEGER frequency, t1, t2;          ///< t1, t2 store high accuracy time between 2 frames: t1 < t2

constexpr int GRID_SIZE = 8;        ///< reference grid size
constexpr float GRID_DEPTH = -5;    ///< how far down is the grid w/r/t origin

bool showPoints = true;               ///< flag which marks if point of cloth are shown or not
bool showStructuralSprings = true;    ///< flag to show structuralSprings
bool showShearSprings = false;        ///< flag to show shearSprings
bool showBendSprings = false;         ///< flag to show bendSprings
int mouseSelectedIndex = -1;          ///< if != -1, stores index of mouse selected point

int prevX = 0;    ///<   Stores previous X coordinate of mouse, used for zooming and rotating the scene
int prevY = 0;    ///< Stores previous Y coordinate of mouse, used for zooming and rotating the scene

float rX = 30;    ///< stores the rotation angle along X for rotating scene
float rY = 0;     ///< stores the rotation angle along Y for rotating scene

bool state = 1;      ///< tells us if we are zooming or rotating
float dist = -25;    ///< stores zoom distance

GLint viewport[4];        ///< Used to set the viewport
GLdouble MV[16];          ///< Used to store the GL_MODELVIEW_MATRIX
GLdouble P[16];           ///< Used to store the GL_PROJECTION_MATRIX
glm::vec3 Up(0, 1, 0);    ///< Up vector of the camera
glm::vec3 Right;          ///< Right vector on the projection plane
glm::vec3 viewDir;        ///< Viewing direction vector

/**
 *  A point struct. This struct is used to encapsulate the points of the cloth
 */
struct Point {
    glm::vec3 pos;                ///< stores the position vector of the point
    glm::vec3 velocity;           ///< stores the velocity vector of the point
    glm::vec3 force;              ///< stores the force vector of the point
    bool isFixedPoint = false;    ///< flag that tells us if the point is invariant stationary or not
};

vector<Point> points;    ///< Array that stores all the points of the cloth

/**
 * Spring-type enum.
 * This enumeration stores the various types of springs in our cloth mesh
 */
enum SPRING_TYPE { STRUCTURAL, SHEAR, BEND };

/**
 * A spring struct. This struct is used to encapsulate the various springs
 * connecting points in our cloth mesh.
 */
struct Spring {
    Point &p1;           ///< reference to first endpoint of our spring
    Point &p2;           ///< reference to second endpoint of our spring
    float restLength;    ///< stores the natural length of the spring
    float Ks;            ///< Spring constant as per Hooke's law: F = -Ks * x
    float Kd;            ///< Damping constant for spring motion: F = -Kd * v
    SPRING_TYPE type;    ///< stores the type of the spring

    /**
     * Constructor function for spring struct
     * @param p1 Reference to end point 1
     * @param p2 Reference to end point 2
     * @param type Spring type
     * @param Ks Spring constant
     * @param Kd Damping constant
     */
    Spring(Point &p1, Point &p2, SPRING_TYPE type, float Ks, float Kd) : p1(p1), p2(p2) {
        this->Ks = Ks;
        this->Kd = Kd;
        this->type = type;
        glm::vec3 diff = p1.pos - p2.pos;
        this->restLength = sqrt(glm::dot(diff, diff));
    }
};

vector<Spring> springs;    ///< Array that stores all the springs in the cloth

/**
 * Setup points and springs
 */
void initGL() {
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&t1);
    glEnable(GL_DEPTH_TEST);

    /**
     * Converts 2D point coordinate into 1D row major index
     * @param x X-Coordinate of point
     * @param y Y-Coordinate of point
     * @return Row Major index of (x,y)
     */
    auto rowMajor = [&](const int x, const int y) -> int {
        return (N + 1) * y + x;
    };

    points.reserve(pointCount);
    for (int j = 0; j <= N; j++) {
        for (int i = 0; i <= N; i++) {
            Point p = Point();
            p.pos = glm::vec3(i * pointSpacing, j * pointSpacing, 0.0f);
            p.velocity = glm::vec3(0);
            if (rowMajor(i, j) == fixedPointOne || rowMajor(i, j) == fixedPointTwo) {
                p.isFixedPoint = true;
            }
            points.push_back(p);
        }
    }

    for (int j = 0; j <= N; j++) {
        for (int i = 0; i < N; i++) {
            Point &p1 = points[rowMajor(i, j)];
            Point &p2 = points[rowMajor(i + 1, j)];
            springs.push_back(Spring(p1, p2, STRUCTURAL, KsStruct, KdStruct));
        }
    }

    for (int j = 0; j < N; j++) {
        for (int i = 0; i <= N; i++) {
            Point &p1 = points[rowMajor(i, j)];
            Point &p2 = points[rowMajor(i, j + 1)];
            springs.push_back(Spring(p1, p2, STRUCTURAL, KsStruct, KdStruct));
        }
    }

    for (int j = 0; j < N; j++) {
        for (int i = 0; i < N; i++) {
            Point &p1 = points[rowMajor(i, j)];
            Point &p2 = points[rowMajor(i + 1, j + 1)];
            springs.push_back(Spring(p1, p2, SHEAR, KsShear, KdShear));
        }
    }

    for (int j = 0; j < N; j++) {
        for (int i = 1; i <= N; i++) {
            Point &p1 = points[rowMajor(i, j)];
            Point &p2 = points[rowMajor(i - 1, j + 1)];
            springs.push_back(Spring(p1, p2, SHEAR, KsShear, KdShear));
        }
    }

    for (int j = 0; j <= N; j++) {
        for (int i = 0; i + 2 <= N; i += 2) {
            Point &p1 = points[rowMajor(i, j)];
            Point &p2 = points[rowMajor(i + 2, j)];
            springs.push_back(Spring(p1, p2, BEND, KsBend, KdBend));
        }
        for (int i = 1; i + 2 <= N; i += 2) {
            Point &p1 = points[rowMajor(i, j)];
            Point &p2 = points[rowMajor(i + 2, j)];
            springs.push_back(Spring(p1, p2, BEND, KsBend, KdBend));
        }
    }

    for (int i = 0; i <= N; i++) {
        for (int j = 0; j + 2 <= N; j += 2) {
            Point &p1 = points[rowMajor(i, j)];
            Point &p2 = points[rowMajor(i, j + 2)];
            springs.push_back(Spring(p1, p2, BEND, KsBend, KdBend));
        }
        for (int j = 1; j + 2 <= N; j += 2) {
            Point &p1 = points[rowMajor(i, j)];
            Point &p2 = points[rowMajor(i, j + 2)];
            springs.push_back(Spring(p1, p2, BEND, KsBend, KdBend));
        }
    }

    ///< Setup the grid to fall by default
    for (auto &p : points) {
        swap(p.pos.z, p.pos.y);
    }

    glPointSize(4);
    wglSwapIntervalEXT(0);
}

/**
 * Render function
 */
void drawScene() {
    QueryPerformanceCounter(&t2);
    accumulator += (t2.QuadPart - t1.QuadPart) * 1000.0 / frequency.QuadPart;
    t1 = t2;

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
        glVertex3f((float) i, GRID_DEPTH, (float) -GRID_SIZE);
        glVertex3f((float) i, GRID_DEPTH, (float) GRID_SIZE);
        glVertex3f((float) -GRID_SIZE, GRID_DEPTH, (float) i);
        glVertex3f((float) GRID_SIZE, GRID_DEPTH, (float) i);
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
        if (s.type == STRUCTURAL and showStructuralSprings) {
            glColor3f(0.0f, 1.0f, 0.0f);
            glVertex3f(s.p1.pos.x, s.p1.pos.y, s.p1.pos.z);
            glVertex3f(s.p2.pos.x, s.p2.pos.y, s.p2.pos.z);
        }
        if (s.type == SHEAR and showShearSprings) {
            glColor3f(248 / 255.0f, 249 / 255.0f, 136 / 255.0f);
            glVertex3f(s.p1.pos.x, s.p1.pos.y, s.p1.pos.z);
            glVertex3f(s.p2.pos.x, s.p2.pos.y, s.p2.pos.z);
        }
        if (s.type == BEND and showBendSprings) {
            glColor3f(107 / 255.0f, 102 / 255.0f, 157 / 255.0f);
            glVertex3f(s.p1.pos.x, s.p1.pos.y, s.p1.pos.z);
            glVertex3f(s.p2.pos.x, s.p2.pos.y, s.p2.pos.z);
        }
    }
    glEnd();

    glutSwapBuffers();
}

/**
 * Function to recalculate the forces on every point
 */
void computeForces() {
    for (auto &p : points) {
        auto &V = p.velocity;
        auto &F = p.force;
        F = glm::vec3(0);

        if (!p.isFixedPoint) F += gravity;    ///< add gravity force only for non-fixed points

        F += DEFAULT_DAMPING * V;    ///< add force due to damping of velocity
    }

    /// <summary>
    /// Add spring forces
    /// </summary>
    for (auto &s : springs) {
        auto &P1 = s.p1.pos;
        auto &V1 = s.p1.velocity;
        auto &F1 = s.p1.force;
        auto &P2 = s.p2.pos;
        auto &V2 = s.p2.velocity;
        auto &F2 = s.p2.force;

        glm::vec3 deltaP = P1 - P2;
        glm::vec3 deltaV = V1 - V2;
        float dist = glm::length(deltaP);

        float leftTerm = -s.Ks * (dist - s.restLength);
        float rightTerm = s.Kd * (glm::dot(deltaV, deltaP) / dist);
        glm::vec3 springForce = (leftTerm + rightTerm) * glm::normalize(deltaP);

        if (!s.p1.isFixedPoint) F1 += springForce;
        if (!s.p2.isFixedPoint) F2 -= springForce;
    }
}

/**
 * Function to use Explicit Euler Integration to compute position and velocity changes
 */
void eulerIntegrate() {
    for (auto &p : points) {
        auto &P = p.pos;
        auto &V = p.velocity;
        auto &F = p.force;
        P += V * timeStep;
        V += (F * timeStep) / mass;
    }
}

/**
 * Function that uses the Jakobsen method to enforce positional constraints
 * https://owlree.blog/posts/simulating-a-rope.html, section 1.2.5
 */
void applyJakobsen() {
    for (auto &s : springs) {
        auto &P1 = s.p1.pos;
        auto &V1 = s.p1.velocity;
        auto &P2 = s.p2.pos;
        auto &V2 = s.p2.velocity;
        glm::vec3 deltaP = P2 - P1;
        float dist = glm::length(deltaP);
        if (dist > s.restLength) {
            dist -= (s.restLength);
            dist /= 2.0f;
            deltaP = glm::normalize(deltaP);
            deltaP *= dist;
            if (s.p1.isFixedPoint) {
                V2 -= deltaP;
            } else if (s.p2.isFixedPoint) {
                V1 += deltaP;
            } else {
                V2 -= deltaP;
                V1 += deltaP;
            }
        }
    }
}

/**
 * Function called in between frame renders
 */
void OnIdle() {
    if (accumulator >= timeStep) {
        accumulator -= timeStep;
        computeForces();
        eulerIntegrate();
        applyJakobsen();
    }
    glutPostRedisplay();
}

/**
 * OpenGL window reshape routine.
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

/**
 * Mouse Click Handler
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

/**
 * Mouse drag handler
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
        float delta = 2000 / abs(dist);
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
        P.y = P.y + Up[1] * valY;
        P.z += Right[2] * valX + Up[2] * valY;
    }
    prevX = x;
    prevY = y;
    glutPostRedisplay();
}

/**
 * Key press handler
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

/**
 * Print options to user
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
