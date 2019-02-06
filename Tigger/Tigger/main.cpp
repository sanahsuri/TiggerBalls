
#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#if defined(__APPLE__)
#include <GLUT/GLUT.h>
#include <OpenGL/gl3.h>
#include <OpenGL/glu.h>
#else
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#include <windows.h>
#endif
#include <GL/glew.h>
#include <GL/freeglut.h>
#endif

#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
const unsigned int windowWidth = 512, windowHeight = 512;

int majorVersion = 3, minorVersion = 0;

bool keyboardState[256];
float DT = 0.0;
int counter = 0;
bool play = true;

enum OBJECT_TYPE { TIGGER, TREE, GROUND, BULLET, BOMB };

void getErrorInfo(unsigned int handle)
{
    int logLen;
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLen);
    if (logLen > 0)
    {
        char * log = new char[logLen];
        int written;
        glGetShaderInfoLog(handle, logLen, &written, log);
        printf("Shader log:\n%s", log);
        delete log;
    }
}

void checkShader(unsigned int shader, const char * message)
{
    int OK;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &OK);
    if (!OK)
    {
        printf("%s!\n", message);
        getErrorInfo(shader);
    }
}

void checkLinking(unsigned int program)
{
    int OK;
    glGetProgramiv(program, GL_LINK_STATUS, &OK);
    if (!OK)
    {
        printf("Failed to link shader program!\n");
        getErrorInfo(program);
    }
}

// row-major matrix 4x4
struct mat4
{
    float m[4][4];
public:
    mat4() {}
    mat4(float m00, float m01, float m02, float m03,
         float m10, float m11, float m12, float m13,
         float m20, float m21, float m22, float m23,
         float m30, float m31, float m32, float m33)
    {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
        m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
        m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;
    }
    
    mat4 operator*(const mat4& right)
    {
        mat4 result;
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                result.m[i][j] = 0;
                for (int k = 0; k < 4; k++) result.m[i][j] += m[i][k] * right.m[k][j];
            }
        }
        return result;
    }
    operator float*() { return &m[0][0]; }
};


// 3D point in homogeneous coordinates
struct vec4
{
    float v[4];
    
    vec4(float x = 0, float y = 0, float z = 0, float w = 1)
    {
        v[0] = x; v[1] = y; v[2] = z; v[3] = w;
    }
    
    vec4 operator*(const mat4& mat)
    {
        vec4 result;
        for (int j = 0; j < 4; j++)
        {
            result.v[j] = 0;
            for (int i = 0; i < 4; i++) result.v[j] += v[i] * mat.m[i][j];
        }
        return result;
    }
    
    vec4 operator+(const vec4& vec)
    {
        vec4 result(v[0] + vec.v[0], v[1] + vec.v[1], v[2] + vec.v[2], v[3] + vec.v[3]);
        return result;
    }
};

// 2D point in Cartesian coordinates
struct vec2
{
    float x, y;
    
    vec2(float x = 0.0, float y = 0.0) : x(x), y(y) {}
    
    vec2 operator+(const vec2& v)
    {
        return vec2(x + v.x, y + v.y);
    }
    
    vec2 operator*(float s)
    {
        return vec2(x * s, y * s);
    }
    
};

// 3D point in Cartesian coordinates
struct vec3
{
    float x, y, z;
    
    vec3(float x = 0.0, float y = 0.0, float z = 0.0) : x(x), y(y), z(z) {}
    
    static vec3 random() { return vec3(((float)rand() / RAND_MAX) * 2 - 1, ((float)rand() / RAND_MAX) * 2 - 1, ((float)rand() / RAND_MAX) * 2 - 1); }
    
    vec3 operator+(const vec3& v) { return vec3(x + v.x, y + v.y, z + v.z); }
    
    vec3 operator-(const vec3& v) { return vec3(x - v.x, y - v.y, z - v.z); }
    
    vec3 operator*(float s) { return vec3(x * s, y * s, z * s); }
    
    vec3 operator/(float s) { return vec3(x / s, y / s, z / s); }
    
    float length() { return sqrt(x * x + y * y + z * z); }
    
    vec3 normalize() { return *this / length(); }
    
    void print() { printf("%f \t %f \t %f \n", x, y, z); }
};

vec3 cross(const vec3& a, const vec3& b)
{
    return vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}



class Geometry
{
protected:
    unsigned int vao;
    
public:
    Geometry()
    {
        glGenVertexArrays(1, &vao);
    }
    
    virtual void Draw() = 0;
};

class TexturedQuad : public Geometry
{
    unsigned int vbo[3];
    
public:
    TexturedQuad()
    {
        glBindVertexArray(vao);
        glGenBuffers(3, vbo);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
        // 0, 0 .. -1, -1 .. -1, 1 // 0, 0 .. -1, 1 .. 1, 1 // 0, 0 .. 1, 1 .. 1, -1 // 0, 0 .. 1, -1, -1, -1
        static float vertexCoords[] =
        {0, 0, 0, 1, -1, 0, -1, 0, -1, 0, 1, 0,   0, 0, 0, 1, -1, 0, 1, 0, 1, 0, 1, 0,   0, 0, 0, 1, 1, 0, 1, 0,1, 0, -1, 0,      0, 0, 0, 1, 1, 0, -1, 0, -1, 0, -1, 0};
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords), vertexCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
        static float vertexTexCoords[] = {0, 0, 1, 0, 0, 1, 1, 1};
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexTexCoords), vertexTexCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
        static float normalCoords[] = { 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0 };
        glBufferData(GL_ARRAY_BUFFER, sizeof(normalCoords), normalCoords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, NULL);
        
    }
    
    void Draw()
    {
        glEnable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 24);
        glDisable(GL_DEPTH_TEST);
    }
};


class   PolygonalMesh : public Geometry
{
    struct  Face
    {
        int       positionIndices[4];
        int       normalIndices[4];
        int       texcoordIndices[4];
        bool      isQuad;
    };
    
    std::vector<std::string*> rows;
    std::vector<vec3*> positions;
    std::vector<std::vector<Face*>> submeshFaces;
    std::vector<vec3*> normals;
    std::vector<vec2*> texcoords;
    
    int nTriangles;
    
public:
    PolygonalMesh(const char *filename);
    ~PolygonalMesh();
    
    void Draw();
};



PolygonalMesh::PolygonalMesh(const char *filename)
{
    std::fstream file(filename);
    if (!file.is_open())
    {
        return;
    }
    
    char buffer[256];
    while (!file.eof())
    {
        file.getline(buffer, 256);
        rows.push_back(new std::string(buffer));
    }
    
    submeshFaces.push_back(std::vector<Face*>());
    std::vector<Face*>* faces = &submeshFaces.at(submeshFaces.size() - 1);
    
    for (int i = 0; i < rows.size(); i++)
    {
        if (rows[i]->empty() || (*rows[i])[0] == '#')
            continue;
        else if ((*rows[i])[0] == 'v' && (*rows[i])[1] == ' ')
        {
            float tmpx, tmpy, tmpz;
            sscanf(rows[i]->c_str(), "v %f %f %f", &tmpx, &tmpy, &tmpz);
            positions.push_back(new vec3(tmpx, tmpy, tmpz));
        }
        else if ((*rows[i])[0] == 'v' && (*rows[i])[1] == 'n')
        {
            float tmpx, tmpy, tmpz;
            sscanf(rows[i]->c_str(), "vn %f %f %f", &tmpx, &tmpy, &tmpz);
            normals.push_back(new vec3(tmpx, tmpy, tmpz));
        }
        else if ((*rows[i])[0] == 'v' && (*rows[i])[1] == 't')
        {
            float tmpx, tmpy;
            sscanf(rows[i]->c_str(), "vt %f %f", &tmpx, &tmpy);
            texcoords.push_back(new vec2(tmpx, tmpy));
        }
        else if ((*rows[i])[0] == 'f')
        {
            if (count(rows[i]->begin(), rows[i]->end(), ' ') == 3)
            {
                Face* f = new Face();
                f->isQuad = false;
                sscanf(rows[i]->c_str(), "f %d/%d/%d %d/%d/%d %d/%d/%d",
                       &f->positionIndices[0], &f->texcoordIndices[0], &f->normalIndices[0],
                       &f->positionIndices[1], &f->texcoordIndices[1], &f->normalIndices[1],
                       &f->positionIndices[2], &f->texcoordIndices[2], &f->normalIndices[2]);
                faces->push_back(f);
            }
            else
            {
                Face* f = new Face();
                f->isQuad = true;
                sscanf(rows[i]->c_str(), "f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d",
                       &f->positionIndices[0], &f->texcoordIndices[0], &f->normalIndices[0],
                       &f->positionIndices[1], &f->texcoordIndices[1], &f->normalIndices[1],
                       &f->positionIndices[2], &f->texcoordIndices[2], &f->normalIndices[2],
                       &f->positionIndices[3], &f->texcoordIndices[3], &f->normalIndices[3]);
                faces->push_back(f);
            }
        }
        else if ((*rows[i])[0] == 'g')
        {
            if (faces->size() > 0)
            {
                submeshFaces.push_back(std::vector<Face*>());
                faces = &submeshFaces.at(submeshFaces.size() - 1);
            }
        }
    }
    
    int numberOfTriangles = 0;
    for (int iSubmesh = 0; iSubmesh < submeshFaces.size(); iSubmesh++)
    {
        std::vector<Face*>& faces = submeshFaces.at(iSubmesh);
        
        for (int i = 0; i < faces.size(); i++)
        {
            if (faces[i]->isQuad) numberOfTriangles += 2;
            else numberOfTriangles += 1;
        }
    }
    
    nTriangles = numberOfTriangles;
    
    float *vertexCoords = new float[numberOfTriangles * 9];
    float *vertexTexCoords = new float[numberOfTriangles * 6];
    float *vertexNormalCoords = new float[numberOfTriangles * 9];
    
    
    int triangleIndex = 0;
    for (int iSubmesh = 0; iSubmesh < submeshFaces.size(); iSubmesh++)
    {
        std::vector<Face*>& faces = submeshFaces.at(iSubmesh);
        
        for (int i = 0; i < faces.size(); i++)
        {
            if (faces[i]->isQuad)
            {
                vertexTexCoords[triangleIndex * 6] = texcoords[faces[i]->texcoordIndices[0] - 1]->x;
                vertexTexCoords[triangleIndex * 6 + 1] = 1 - texcoords[faces[i]->texcoordIndices[0] - 1]->y;
                
                vertexTexCoords[triangleIndex * 6 + 2] = texcoords[faces[i]->texcoordIndices[1] - 1]->x;
                vertexTexCoords[triangleIndex * 6 + 3] = 1 - texcoords[faces[i]->texcoordIndices[1] - 1]->y;
                
                vertexTexCoords[triangleIndex * 6 + 4] = texcoords[faces[i]->texcoordIndices[2] - 1]->x;
                vertexTexCoords[triangleIndex * 6 + 5] = 1 - texcoords[faces[i]->texcoordIndices[2] - 1]->y;
                
                
                vertexCoords[triangleIndex * 9] = positions[faces[i]->positionIndices[0] - 1]->x;
                vertexCoords[triangleIndex * 9 + 1] = positions[faces[i]->positionIndices[0] - 1]->y;
                vertexCoords[triangleIndex * 9 + 2] = positions[faces[i]->positionIndices[0] - 1]->z;
                
                vertexCoords[triangleIndex * 9 + 3] = positions[faces[i]->positionIndices[1] - 1]->x;
                vertexCoords[triangleIndex * 9 + 4] = positions[faces[i]->positionIndices[1] - 1]->y;
                vertexCoords[triangleIndex * 9 + 5] = positions[faces[i]->positionIndices[1] - 1]->z;
                
                vertexCoords[triangleIndex * 9 + 6] = positions[faces[i]->positionIndices[2] - 1]->x;
                vertexCoords[triangleIndex * 9 + 7] = positions[faces[i]->positionIndices[2] - 1]->y;
                vertexCoords[triangleIndex * 9 + 8] = positions[faces[i]->positionIndices[2] - 1]->z;
                
                
                vertexNormalCoords[triangleIndex * 9] = normals[faces[i]->normalIndices[0] - 1]->x;
                vertexNormalCoords[triangleIndex * 9 + 1] = normals[faces[i]->normalIndices[0] - 1]->y;
                vertexNormalCoords[triangleIndex * 9 + 2] = normals[faces[i]->normalIndices[0] - 1]->z;
                
                vertexNormalCoords[triangleIndex * 9 + 3] = normals[faces[i]->normalIndices[1] - 1]->x;
                vertexNormalCoords[triangleIndex * 9 + 4] = normals[faces[i]->normalIndices[1] - 1]->y;
                vertexNormalCoords[triangleIndex * 9 + 5] = normals[faces[i]->normalIndices[1] - 1]->z;
                
                vertexNormalCoords[triangleIndex * 9 + 6] = normals[faces[i]->normalIndices[2] - 1]->x;
                vertexNormalCoords[triangleIndex * 9 + 7] = normals[faces[i]->normalIndices[2] - 1]->y;
                vertexNormalCoords[triangleIndex * 9 + 8] = normals[faces[i]->normalIndices[2] - 1]->z;
                
                triangleIndex++;
                
                
                vertexTexCoords[triangleIndex * 6] = texcoords[faces[i]->texcoordIndices[1] - 1]->x;
                vertexTexCoords[triangleIndex * 6 + 1] = 1 - texcoords[faces[i]->texcoordIndices[1] - 1]->y;
                
                vertexTexCoords[triangleIndex * 6 + 2] = texcoords[faces[i]->texcoordIndices[2] - 1]->x;
                vertexTexCoords[triangleIndex * 6 + 3] = 1 - texcoords[faces[i]->texcoordIndices[2] - 1]->y;
                
                vertexTexCoords[triangleIndex * 6 + 4] = texcoords[faces[i]->texcoordIndices[3] - 1]->x;
                vertexTexCoords[triangleIndex * 6 + 5] = 1 - texcoords[faces[i]->texcoordIndices[3] - 1]->y;
                
                
                vertexCoords[triangleIndex * 9] = positions[faces[i]->positionIndices[1] - 1]->x;
                vertexCoords[triangleIndex * 9 + 1] = positions[faces[i]->positionIndices[1] - 1]->y;
                vertexCoords[triangleIndex * 9 + 2] = positions[faces[i]->positionIndices[1] - 1]->z;
                
                vertexCoords[triangleIndex * 9 + 3] = positions[faces[i]->positionIndices[2] - 1]->x;
                vertexCoords[triangleIndex * 9 + 4] = positions[faces[i]->positionIndices[2] - 1]->y;
                vertexCoords[triangleIndex * 9 + 5] = positions[faces[i]->positionIndices[2] - 1]->z;
                
                vertexCoords[triangleIndex * 9 + 6] = positions[faces[i]->positionIndices[3] - 1]->x;
                vertexCoords[triangleIndex * 9 + 7] = positions[faces[i]->positionIndices[3] - 1]->y;
                vertexCoords[triangleIndex * 9 + 8] = positions[faces[i]->positionIndices[3] - 1]->z;
                
                
                vertexNormalCoords[triangleIndex * 9] = normals[faces[i]->normalIndices[1] - 1]->x;
                vertexNormalCoords[triangleIndex * 9 + 1] = normals[faces[i]->normalIndices[1] - 1]->y;
                vertexNormalCoords[triangleIndex * 9 + 2] = normals[faces[i]->normalIndices[1] - 1]->z;
                
                vertexNormalCoords[triangleIndex * 9 + 3] = normals[faces[i]->normalIndices[2] - 1]->x;
                vertexNormalCoords[triangleIndex * 9 + 4] = normals[faces[i]->normalIndices[2] - 1]->y;
                vertexNormalCoords[triangleIndex * 9 + 5] = normals[faces[i]->normalIndices[2] - 1]->z;
                
                vertexNormalCoords[triangleIndex * 9 + 6] = normals[faces[i]->normalIndices[3] - 1]->x;
                vertexNormalCoords[triangleIndex * 9 + 7] = normals[faces[i]->normalIndices[3] - 1]->y;
                vertexNormalCoords[triangleIndex * 9 + 8] = normals[faces[i]->normalIndices[3] - 1]->z;
                
                triangleIndex++;
            }
            else
            {
                vertexTexCoords[triangleIndex * 6] = texcoords[faces[i]->texcoordIndices[0] - 1]->x;
                vertexTexCoords[triangleIndex * 6 + 1] = 1 - texcoords[faces[i]->texcoordIndices[0] - 1]->y;
                
                vertexTexCoords[triangleIndex * 6 + 2] = texcoords[faces[i]->texcoordIndices[1] - 1]->x;
                vertexTexCoords[triangleIndex * 6 + 3] = 1 - texcoords[faces[i]->texcoordIndices[1] - 1]->y;
                
                vertexTexCoords[triangleIndex * 6 + 4] = texcoords[faces[i]->texcoordIndices[2] - 1]->x;
                vertexTexCoords[triangleIndex * 6 + 5] = 1 - texcoords[faces[i]->texcoordIndices[2] - 1]->y;
                
                vertexCoords[triangleIndex * 9] = positions[faces[i]->positionIndices[0] - 1]->x;
                vertexCoords[triangleIndex * 9 + 1] = positions[faces[i]->positionIndices[0] - 1]->y;
                vertexCoords[triangleIndex * 9 + 2] = positions[faces[i]->positionIndices[0] - 1]->z;
                
                vertexCoords[triangleIndex * 9 + 3] = positions[faces[i]->positionIndices[1] - 1]->x;
                vertexCoords[triangleIndex * 9 + 4] = positions[faces[i]->positionIndices[1] - 1]->y;
                vertexCoords[triangleIndex * 9 + 5] = positions[faces[i]->positionIndices[1] - 1]->z;
                
                vertexCoords[triangleIndex * 9 + 6] = positions[faces[i]->positionIndices[2] - 1]->x;
                vertexCoords[triangleIndex * 9 + 7] = positions[faces[i]->positionIndices[2] - 1]->y;
                vertexCoords[triangleIndex * 9 + 8] = positions[faces[i]->positionIndices[2] - 1]->z;
                
                
                vertexNormalCoords[triangleIndex * 9] = normals[faces[i]->normalIndices[0] - 1]->x;
                vertexNormalCoords[triangleIndex * 9 + 1] = normals[faces[i]->normalIndices[0] - 1]->y;
                vertexNormalCoords[triangleIndex * 9 + 2] = normals[faces[i]->normalIndices[0] - 1]->z;
                
                vertexNormalCoords[triangleIndex * 9 + 3] = normals[faces[i]->normalIndices[1] - 1]->x;
                vertexNormalCoords[triangleIndex * 9 + 4] = normals[faces[i]->normalIndices[1] - 1]->y;
                vertexNormalCoords[triangleIndex * 9 + 5] = normals[faces[i]->normalIndices[1] - 1]->z;
                
                vertexNormalCoords[triangleIndex * 9 + 6] = normals[faces[i]->normalIndices[2] - 1]->x;
                vertexNormalCoords[triangleIndex * 9 + 7] = normals[faces[i]->normalIndices[2] - 1]->y;
                vertexNormalCoords[triangleIndex * 9 + 8] = normals[faces[i]->normalIndices[2] - 1]->z;
                
                triangleIndex++;
            }
        }
    }
    
    glBindVertexArray(vao);
    
    unsigned int vbo[3];
    glGenBuffers(3, &vbo[0]);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, nTriangles * 9 * sizeof(float), vertexCoords, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, nTriangles * 6 * sizeof(float), vertexTexCoords, GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
    glBufferData(GL_ARRAY_BUFFER, nTriangles * 9 * sizeof(float), vertexNormalCoords, GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    
    delete vertexCoords;
    delete vertexTexCoords;
    delete vertexNormalCoords;
}


void PolygonalMesh::Draw()
{
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, nTriangles * 3);
    glDisable(GL_DEPTH_TEST);
}


PolygonalMesh::~PolygonalMesh()
{
    for (unsigned int i = 0; i < rows.size(); i++) delete rows[i];
    for (unsigned int i = 0; i < positions.size(); i++) delete positions[i];
    for (unsigned int i = 0; i < submeshFaces.size(); i++)
        for (unsigned int j = 0; j < submeshFaces.at(i).size(); j++)
            delete submeshFaces.at(i).at(j);
    for (unsigned int i = 0; i < normals.size(); i++) delete normals[i];
    for (unsigned int i = 0; i < texcoords.size(); i++) delete texcoords[i];
}



class Shader
{
protected:
    unsigned int shaderProgram;
    
public:
    Shader()
    {
        shaderProgram = 0;
    }
    
    ~Shader()
    {
        if (shaderProgram) glDeleteProgram(shaderProgram);
    }
    
    void Run()
    {
        if (shaderProgram) glUseProgram(shaderProgram);
    }
    
    virtual void UploadInvM(mat4& InVM) { }
    
    virtual void UploadMVP(mat4& MVP) { }
    
    virtual void UploadColor(vec4& color) { }
    
    virtual void UploadSamplerID() { }
    
    virtual void UploadMaterialAttributes(vec3& ka, vec3& kd, vec3& ks, float shininess) { }
    
    virtual void UploadLightAttributes(vec3& La, vec3& Le, vec4& worldLightPosition) { }
    
    virtual void UploadM(mat4& M) { }
    
    virtual void UploadVP(mat4& VP) { }
    
    virtual void UploadEyePosition(vec3& wEye) { }
};

class ShadowShader : public Shader
{
public:
    ShadowShader()
    {
        const char *vertexSource = R"(
#version 150
        precision highp float;
        in vec3 vertexPosition;
        in vec2 vertexTexCoord;
        in vec3 vertexNormal;
        uniform mat4 M, VP;
        uniform vec4 worldLightPosition;
        void main() {
            vec4 p = vec4(vertexPosition, 1) * M;
            vec3 s;
            s.y = -0.999;
            s.x = (p.x - worldLightPosition.x) / (p.y - worldLightPosition.y) * (s.y - worldLightPosition.y) + worldLightPosition.x;
            s.z = (p.z - worldLightPosition.z) / (p.y - worldLightPosition.y) * (s.y - worldLightPosition.y) + worldLightPosition.z;
            gl_Position = vec4(s, 1) * VP;
        }
        )";

        
        const char *fragmentSource = R"(
#version 150
        precision highp float;
        out vec4 fragmentColor;
        void main()
        {
            fragmentColor = vec4(0.0, 0.1, 0.0, 1);
        }
        )";
        
        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        if (!vertexShader) { printf("Error in vertex shader creation\n"); exit(1); }
        
        glShaderSource(vertexShader, 1, &vertexSource, NULL);
        glCompileShader(vertexShader);
        checkShader(vertexShader, "Vertex shader error");
        
        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        if (!fragmentShader) { printf("Error in fragment shader creation\n"); exit(1); }
        
        glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
        glCompileShader(fragmentShader);
        checkShader(fragmentShader, "Fragment shader error");
        
        shaderProgram = glCreateProgram();
        if (!shaderProgram) { printf("Error in shader program creation\n"); exit(1); }
        
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        
        glBindAttribLocation(shaderProgram, 0, "vertexPosition");
        glBindAttribLocation(shaderProgram, 1, "vertexTexCoord");
        glBindAttribLocation(shaderProgram, 2, "vertexNormal");
        
        glBindFragDataLocation(shaderProgram, 0, "fragmentColor");
        
        glLinkProgram(shaderProgram);
        checkLinking(shaderProgram);
    }
    
    virtual void UploadM(mat4& M)
    {
        int location = glGetUniformLocation(shaderProgram, "M");
        if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, M);
        else printf("uniform M cannot be set\n");
    }
    
    void UploadVP(mat4& VP)
    {
        int location = glGetUniformLocation(shaderProgram, "VP");
        if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, VP);
        else printf("uniform VP cannot be set\n");
    }
    
    void UploadLightAttributes(vec3& La, vec3& Le, vec4& worldLightPosition)
    {
        int location = glGetUniformLocation(shaderProgram, "worldLightPosition");
        if (location >= 0) glUniform4fv(location, 1, &worldLightPosition.v[0]);
        else printf("uniform worldLightPosition cannot be set\n");
    }
};

class InfiniteQuadShader : public Shader
{
public:
    InfiniteQuadShader()
    {
        const char *vertexSource = R"(
#version 150
        precision highp float;
        in vec4 vertexPosition;
        in vec2 vertexTexCoord;
        in vec3 vertexNormal;
        uniform mat4 M, InvM, MVP;
        out vec2 texCoord;
        out vec4 worldPosition;
        out vec3 worldNormal;
        void main() {
            texCoord = vertexTexCoord;
            worldPosition = vertexPosition * M;
            worldNormal = (InvM * vec4(vertexNormal, 0.0)).xyz;
            gl_Position = vertexPosition * MVP;
        }
        )";
        
        const char *fragmentSource = R"(
#version 150
        precision highp float;
        uniform sampler2D samplerUnit;
        uniform vec3 La, Le;
        uniform vec3 ka, kd, ks;
        uniform float shininess;
        uniform vec3 worldEyePosition;
        uniform vec4 worldLightPosition;
        in vec2 texCoord;
        in vec4 worldPosition;
        in vec3 worldNormal;
        out vec4 fragmentColor;
        void main() {
            vec3 N = normalize(worldNormal);
            vec3 V = normalize(worldEyePosition * worldPosition.w - worldPosition.xyz);
            vec3 L = normalize(worldLightPosition.xyz * worldPosition.w - worldPosition.xyz * worldLightPosition.w);
            vec3 H = normalize(V + L);
            vec2 position = worldPosition.xz / worldPosition.w;
            vec2 tex = position.xy - floor(position.xy);
            vec3 texel = texture(samplerUnit, tex).xyz;
            vec3 color = La * ka + Le * kd * texel * max(0.0, dot(L, N)) + Le * ks * pow(max(0.0, dot(H, N)), shininess);
            fragmentColor = vec4(color, 1);
        }
        )";
        
        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        if (!vertexShader) { printf("Error in vertex shader creation\n"); exit(1); }
        
        glShaderSource(vertexShader, 1, &vertexSource, NULL);
        glCompileShader(vertexShader);
        checkShader(vertexShader, "Vertex shader error");
        
        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        if (!fragmentShader) { printf("Error in fragment shader creation\n"); exit(1); }
        
        glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
        glCompileShader(fragmentShader);
        checkShader(fragmentShader, "Fragment shader error");
        
        shaderProgram = glCreateProgram();
        if (!shaderProgram) { printf("Error in shader program creation\n"); exit(1); }
        
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        
        glBindAttribLocation(shaderProgram, 0, "vertexPosition");
        glBindAttribLocation(shaderProgram, 1, "vertexTexCoord");
        glBindAttribLocation(shaderProgram, 2, "vertexNormal");
        
        glBindFragDataLocation(shaderProgram, 0, "fragmentColor");
        
        glLinkProgram(shaderProgram);
        checkLinking(shaderProgram);
    }
    
    void UploadSamplerID()
    {
        int samplerUnit = 0;
        int location = glGetUniformLocation(shaderProgram, "samplerUnit");
        glUniform1i(location, samplerUnit);
        glActiveTexture(GL_TEXTURE0 + samplerUnit);
    }
    
    void UploadInvM(mat4& InvM)
    {
        int location = glGetUniformLocation(shaderProgram, "InvM");
        if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, InvM);
        else printf("uniform InvM cannot be set\n");
    }
    
    void UploadMVP(mat4& MVP)
    {
        int location = glGetUniformLocation(shaderProgram, "MVP");
        if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, MVP);
        else printf("uniform MVP cannot be set\n");
    }
    
    virtual void UploadM(mat4& M)
    {
        int location = glGetUniformLocation(shaderProgram, "M");
        if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, M);
        else printf("uniform M cannot be set\n");
    }
    
    void UploadMaterialAttributes(vec3& ka, vec3& kd, vec3& ks, float shininess)
    {
        int location = glGetUniformLocation(shaderProgram, "ka");
        if (location >= 0) glUniform3fv(location, 1, &ka.x);
        else printf("uniform ka cannot be set\n");
        
        location = glGetUniformLocation(shaderProgram, "kd");
        if (location >= 0) glUniform3fv(location, 1, &kd.x);
        else printf("uniform kd cannot be set\n");
        
        location = glGetUniformLocation(shaderProgram, "ks");
        if (location >= 0) glUniform3fv(location, 1, &ks.x);
        else printf("uniform ks cannot be set\n");
        
        location = glGetUniformLocation(shaderProgram, "shininess");
        if (location >= 0) glUniform1f(location, shininess);
        else printf("uniform shininess cannot be set\n");
    }
    
    void UploadLightAttributes(vec3& La, vec3& Le, vec4& worldLightPosition)
    {
        int location = glGetUniformLocation(shaderProgram, "La");
        if (location >= 0) glUniform3fv(location, 1, &La.x);
        else printf("uniform La cannot be set\n");
        
        location = glGetUniformLocation(shaderProgram, "Le");
        if (location >= 0) glUniform3fv(location, 1, &Le.x);
        else printf("uniform Le cannot be set\n");
        
        location = glGetUniformLocation(shaderProgram, "worldLightPosition");
        if (location >= 0) glUniform4fv(location, 1, &worldLightPosition.v[0]);
        else printf("uniform ka cannot be set\n");
    }
    
    void UploadEyePosition(vec3& eye)
    {
        int location = glGetUniformLocation(shaderProgram, "worldEyePosition");
        if (location >= 0) glUniform3fv(location, 1, &eye.x);
        else printf("uniform wEye cannot be set\n");
    }
    
};


class MeshShader : public Shader
{
public:
    MeshShader()
    {
        
        const char *vertexSource = R"(
#version 150
        precision highp float;
        in vec3 vertexPosition;
        in vec2 vertexTexCoord;
        in vec3 vertexNormal;
        uniform mat4 M, InvM, MVP;
        uniform vec3 worldEyePosition;
        uniform vec4 worldLightPosition;
        out vec2 texCoord;
        out vec3 worldNormal;
        out vec3 worldView;
        out vec3 worldLight;
        
        void main() {
            texCoord = vertexTexCoord;
            vec4 worldPosition = vec4(vertexPosition, 1) * M;
            worldLight  = worldLightPosition.xyz * worldPosition.w - worldPosition.xyz * worldLightPosition.w;
            worldView = worldEyePosition - worldPosition.xyz;
            worldNormal = (InvM * vec4(vertexNormal, 0.0)).xyz;
            gl_Position = vec4(vertexPosition, 1) * MVP;
        }
        )";
        
        
        const char *fragmentSource = R"(
#version 150
        precision highp float;
        uniform sampler2D samplerUnit;
        uniform vec3 La, Le;
        uniform vec3 ka, kd, ks;
        uniform float shininess;
        in vec2 texCoord;
        in vec3 worldNormal;
        in vec3 worldView;
        in vec3 worldLight;
        out vec4 fragmentColor;
        
        void main() {
            vec3 N = normalize(worldNormal);
            vec3 V = normalize(worldView);
            vec3 L = normalize(worldLight);
            vec3 H = normalize(V + L);
            vec3 texel = texture(samplerUnit, texCoord).xyz;
            vec3 color =
            La * ka +
            Le * kd * texel * max(0.0, dot(L, N)) +
            Le * ks * pow(max(0.0, dot(H, N)), shininess);
            fragmentColor = vec4(color.xyz, 1);
        }
        )";
        
        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        if (!vertexShader) { printf("Error in vertex shader creation\n"); exit(1); }
        
        glShaderSource(vertexShader, 1, &vertexSource, NULL);
        glCompileShader(vertexShader);
        checkShader(vertexShader, "Vertex shader error");
        
        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        if (!fragmentShader) { printf("Error in fragment shader creation\n"); exit(1); }
        
        glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
        glCompileShader(fragmentShader);
        checkShader(fragmentShader, "Fragment shader error");
        
        shaderProgram = glCreateProgram();
        if (!shaderProgram) { printf("Error in shader program creation\n"); exit(1); }
        
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        
        glBindAttribLocation(shaderProgram, 0, "vertexPosition");
        glBindAttribLocation(shaderProgram, 1, "vertexTexCoord");
        glBindAttribLocation(shaderProgram, 2, "vertexNormal");
        
        glBindFragDataLocation(shaderProgram, 0, "fragmentColor");
        
        glLinkProgram(shaderProgram);
        checkLinking(shaderProgram);
    }
    
    void UploadSamplerID()
    {
        int samplerUnit = 0;
        int location = glGetUniformLocation(shaderProgram, "samplerUnit");
        glUniform1i(location, samplerUnit);
        glActiveTexture(GL_TEXTURE0 + samplerUnit);
    }
    
    void UploadInvM(mat4& InvM)
    {
        int location = glGetUniformLocation(shaderProgram, "InvM");
        if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, InvM);
        else printf("uniform InvM cannot be set\n");
    }
    
    void UploadMVP(mat4& MVP)
    {
        int location = glGetUniformLocation(shaderProgram, "MVP");
        if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, MVP);
        else printf("uniform MVP cannot be set\n");
    }
    
    virtual void UploadM(mat4& M)
    {
        int location = glGetUniformLocation(shaderProgram, "M");
        if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, M);
        else printf("uniform M cannot be set\n");
    }
    
    void UploadMaterialAttributes(vec3& ka, vec3& kd, vec3& ks, float shininess)
    {
        
        int location = glGetUniformLocation(shaderProgram, "ka");
        if (location >= 0) glUniform3fv(location, 1, &ka.x);
        else printf("uniform ka cannot be set\n");
        
        location = glGetUniformLocation(shaderProgram, "kd");
        if (location >= 0) glUniform3fv(location, 1, &kd.x);
        else printf("uniform kd cannot be set\n");
        
        location = glGetUniformLocation(shaderProgram, "ks");
        if (location >= 0) glUniform3fv(location, 1, &ks.x);
        else printf("uniform ks cannot be set\n");
        
        location = glGetUniformLocation(shaderProgram, "shininess");
        if (location >= 0) glUniform1f(location, shininess);
        else printf("uniform shininess cannot be set\n");
    }
    
    void UploadLightAttributes(vec3& La, vec3& Le, vec4& worldLightPosition)
    {
        int location = glGetUniformLocation(shaderProgram, "La");
        if (location >= 0) glUniform3fv(location, 1, &La.x);
        else printf("uniform La cannot be set\n");
        
        location = glGetUniformLocation(shaderProgram, "Le");
        if (location >= 0) glUniform3fv(location, 1, &Le.x);
        else printf("uniform Le cannot be set\n");
        
        location = glGetUniformLocation(shaderProgram, "worldLightPosition");
        if (location >= 0) glUniform4fv(location, 1, &worldLightPosition.v[0]);
        else printf("uniform worldLightPosition cannot be set\n");
    }
    
    void UploadEyePosition(vec3& eye)
    {
        int location = glGetUniformLocation(shaderProgram, "worldEyePosition");
        if (location >= 0) glUniform3fv(location, 1, &eye.x);
        else printf("uniform wEye cannot be set\n");
    }
};

class Light
{
    vec3 La, Le;
    vec4 worldLightPosition;
    
public:
    Light(vec4 worldLightPosition, vec3 Le = vec3(1.0, 1.0, 1.0), vec3 La = vec3(1.0, 1.0, 1.0)) : worldLightPosition(worldLightPosition), Le(Le), La(La)
    {
        
    }
    
    void UploadAttributes(Shader* shader)
    {
        shader->UploadLightAttributes(La, Le, worldLightPosition);
    }
    
    void SetPointLightSource(vec3& pos)
    {
        worldLightPosition.v[0] = pos.x;
        worldLightPosition.v[1] = pos.y;
        worldLightPosition.v[2] = pos.z;
        worldLightPosition.v[3] = 1;
    }
    
    void SetDirectionalLightSource(vec3& dir)
    {
        worldLightPosition.v[0] = dir.x;
        worldLightPosition.v[1] = dir.y;
        worldLightPosition.v[2] = dir.z;
        worldLightPosition.v[3] = 0;
    }
    
    void SetLe(vec3 newLe)
    {
        Le = newLe;
    }
    
    void SetWLP(vec4& WLP)
    {
        worldLightPosition = WLP;
    }
};


extern "C" unsigned char* stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);

class Texture
{
    unsigned int textureId;
    
public:
    Texture(const std::string& inputFileName)
    {
        unsigned char* data;
        int width; int height; int nComponents = 4;
        
        data = stbi_load(inputFileName.c_str(), &width, &height, &nComponents, 0);
        
        if (data == NULL)
        {
            printf("Texture not a thing here");
            return;
        }
        
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        
        if (nComponents == 3) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        if (nComponents == 4) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        delete data;
    }
    
    void Bind()
    {
        glBindTexture(GL_TEXTURE_2D, textureId);
    }
};




class Material
{
    Shader* shader;
    Texture* texture;
    vec3 ka, kd, ks;
    float shininess;
    
public:
    Material(Shader* sh, Texture* t = 0, vec3 ka = vec3(1.0, 1.0, 1.0),
             vec3 kd = vec3(1.0, 1.0, 1.0), vec3 ks = vec3(1.0, 1.0, 1.0), float shininess = 0) : ka(ka), kd(kd), ks(ks), shininess(shininess)
    {
        shader = sh;
        texture = t;
    }
    
    Shader* GetShader() { return shader; }
    
    void UploadAttributes()
    {
        if (texture)
        {
            shader->UploadSamplerID();
            texture->Bind();
            shader->UploadMaterialAttributes(ka, kd, ks, shininess);
        }
        else
            shader->UploadMaterialAttributes(ka, kd, ks, shininess);
    }
};

class Mesh
{
    Geometry* geometry;
    Material* material;
    
public:
    Mesh(Geometry* g, Material* m)
    {
        geometry = g;
        material = m;
    }
    
    Shader* GetShader() { return material->GetShader(); }
    
    void Draw()
    {
        material->UploadAttributes();
        geometry->Draw();
    }
};





class Camera {
    vec3  wEye, wLookat, wVup, velocity;
    float fov, asp, fp, bp, angularVelocity;
    bool quake = false;
    
public:
    Camera()
    {
        wEye = vec3(0.0, 0.0, 2.0);
        wLookat = vec3(0.0, 0.0, 0.0);
        wVup = vec3(0.0, 1.0, 0.0);
        fov = M_PI / 4.0; asp = 1.0; fp = 0.01; bp = 10.0;
        
        velocity = vec3(0.0, 0.0, 0.0);
        angularVelocity = 0.0;
    }
    
    void SetAspectRatio(float a) { asp = a; }
    
    void Quake(float dt) {
        if (counter > 0 && play) {
            // counter in onIdle()
            // counter = 100.0
            // when 0 => shaking stops
            // scene calls move function of camera
            float trig = sin(dt);
            if (counter%2 == 0) {
            wEye = wEye + vec3(trig, 0.0, 0.0) * 5.0;
            wLookat = wLookat + vec3(trig, 0.0, 0.0) * 5.0;
            } else {
                wEye = wEye + vec3(-1 * trig, 0.0, 0.0) * 5.0;
                wLookat = wLookat + vec3(-1 * trig, 0.0, 0.0) * 5.0;
            }
            printf("QUAKE");
        }
    }
    
    void Reset() {
        wEye = vec3(0.0, 0.0, 2.0);
    }
    
    mat4 GetViewMatrix()
    {
        vec3 w = (wEye - wLookat).normalize();
        vec3 u = cross(wVup, w).normalize();
        vec3 v = cross(w, u);
        
        return
        mat4(
             1.0f, 0.0f, 0.0f, 0.0f,
             0.0f, 1.0f, 0.0f, 0.0f,
             0.0f, 0.0f, 1.0f, 0.0f,
             -wEye.x, -wEye.y, -wEye.z, 1.0f) *
        mat4(
             u.x, v.x, w.x, 0.0f,
             u.y, v.y, w.y, 0.0f,
             u.z, v.z, w.z, 0.0f,
             0.0f, 0.0f, 0.0f, 1.0f);
    }
    
    mat4 GetProjectionMatrix()
    {
        float sy = 1 / tan(fov / 2);
        return mat4(
                    sy / asp, 0.0f, 0.0f, 0.0f,
                    0.0f, sy, 0.0f, 0.0f,
                    0.0f, 0.0f, -(fp + bp) / (bp - fp), -1.0f,
                    0.0f, 0.0f, -2 * fp*bp / (bp - fp), 0.0f);
    }
    
    void setQuake(bool b) {
        quake = b;
    }
    
    float GetDistance()
    {
        return (wLookat - wEye).length();
    }
    
    vec3 GetAhead()
    {
        return (wLookat - wEye).normalize();
    }
    
    vec3 GetEyePosition()
    {
        return wEye;
    }
    
    vec3 GetVelocity()
    {
        return velocity;
    }
    
    void SetLookAt(vec3 look)
    {
        wLookat = look;
    }
    
    void SetEye (vec3 eye)
    {
        wEye = eye;
    }
    
    
    void Control()
    {
        if (keyboardState['i'])
        {
            velocity = GetAhead() * 2.0;
            return;
        }
        if (keyboardState['k'])
        {
            velocity = GetAhead() * -2.0;
            return;
        }
        if (keyboardState['l'])
        {
            angularVelocity = 2.0;
            return;
        }
        if (keyboardState['j'])
        {
            angularVelocity = -2.0;
            return;
        }
        
        angularVelocity = 0.0;
        velocity = vec3(0.0, 0.0, 0.0);
        
    }
    
    void Move(float dt)
    {
        // for W and S
        wEye = wEye + velocity * dt;
        wLookat = wLookat + velocity * dt;
        
        // for A and D
        vec3 w = wLookat - wEye;
        float d = w.length();
        w = w.normalize();
        
        vec3 r = cross(GetAhead(), wVup);
        
        w = (w * cos(angularVelocity * dt) + r * sin(angularVelocity * dt)) * d;
        
        wLookat = w + wEye;
    }
    
    void UploadAttributes(Shader* shader)
    {
        shader->UploadEyePosition(wEye);
    }
};

Camera camera;

Light light(vec4(0.0, 0.0, 0.0, 1.0)); // directional
Light spotlight(vec4(0.0, 0.0, 0.0, 0.0)); // point

class Object
{
protected:
    Shader* shader;
    Mesh *mesh;
    
    vec3 position;
    vec3 scaling;
    float orientation;

    vec3 velocity, acceleration;
    float angularVelocity, angularAcceleration;
    
    bool alive = true;
    
public:
    Object(Mesh *m, vec3 position = vec3(0.0, 0.0, 0.0), vec3 scaling = vec3(1.0, 1.0, 1.0), float orientation = 0.0) : position(position), scaling(scaling), orientation(orientation)
    {
        shader = m->GetShader();
        mesh = m;
    }
    
    virtual void setFly() { }
    
    void Fly(float dt) { }
    
    
    virtual void Move(float dt)
    {
     // update velocity, angular velocity, position and orientation using acceleration and angular acceleration
        velocity = velocity + acceleration * dt;
        position = position + velocity * dt;
        angularVelocity = angularVelocity + angularAcceleration * dt;
        orientation = orientation + angularVelocity * dt;

    }
    
    virtual void Interact(Object* object){};
    
    virtual OBJECT_TYPE GetType() = 0;
    
    virtual void Control(float dt) {};
    
    vec3& GetPosition() { return position; }
    
    vec3 GetAvatar()
    {
        float alpha = (orientation + 180) / 180.0 * M_PI;
        vec3 ahead = vec3(cos(alpha), 0.0, sin(alpha));
        return ahead.normalize();
        // multiply by the distance between eye and lookat
    }
    
    virtual void Roatate(float dt) { }
    
    void Draw()
    {
        shader->Run();
        
        UploadAttributes();
        
        vec3 eye = camera.GetEyePosition();
        light.SetPointLightSource(eye);
        vec3 dir = vec3(0.0, 20.0, 15.0);
        light.SetDirectionalLightSource(dir);
        light.UploadAttributes(shader);
        camera.UploadAttributes(shader);
        
        mesh->Draw();
    }
    
    bool isAlive() {
        return alive;
    }
    
    void SetLight(Light spotlight)
    {
        vec3 point = vec3(GetPosition().x, 10.0, GetPosition().z);
        vec3 dir = GetPosition();
        spotlight.SetPointLightSource(point);
        spotlight.SetLe(vec3(3.0, 3.0, 3.0));
        spotlight.SetDirectionalLightSource(dir);
        spotlight.UploadAttributes(shader);
    }
    
    void DrawShadow(Shader* shadowShader)
    {
        shadowShader->Run();
        UploadAttributes(shadowShader);
        
        vec3 dir = vec3(0.0, 100.0, 0.0);
        light.SetPointLightSource(dir);
        light.UploadAttributes(shadowShader);
        
        camera.UploadAttributes(shadowShader);
        
        mesh->Draw();
    }
    
    virtual void UploadAttributes()
    {
        mat4 T = mat4(
                      1.0, 0.0, 0.0, 0.0,
                      0.0, 1.0, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      position.x, position.y, position.z, 1.0);
        
        mat4 InvT = mat4(
                         1.0, 0.0, 0.0, 0.0,
                         0.0, 1.0, 0.0, 0.0,
                         0.0, 0.0, 1.0, 0.0,
                         -position.x, -position.y, -position.z, 1.0);
        
        mat4 S = mat4(
                      scaling.x, 0.0, 0.0, 0.0,
                      0.0, scaling.y, 0.0, 0.0,
                      0.0, 0.0, scaling.z, 0.0,
                      0.0, 0.0, 0.0, 1.0);
        
        mat4 InvS = mat4(
                         1.0 / scaling.x, 0.0, 0.0, 0.0,
                         0.0, 1.0 / scaling.y, 0.0, 0.0,
                         0.0, 0.0, 1.0 / scaling.z, 0.0,
                         0.0, 0.0, 0.0, 1.0);
        
        float alpha = orientation / 180.0 * M_PI;
        
        mat4 R = mat4(
                      cos(alpha), 0.0, sin(alpha), 0.0,
                      0.0, 1.0, 0.0, 0.0,
                      -sin(alpha), 0.0, cos(alpha), 0.0,
                      0.0, 0.0, 0.0, 1.0);
        
        mat4 InvR = mat4(
                         cos(alpha), 0.0, -sin(alpha), 0.0,
                         0.0, 1.0, 0.0, 0.0,
                         sin(alpha), 0.0, cos(alpha), 0.0,
                         0.0, 0.0, 0.0, 1.0);
        
        mat4 M = S * R * T;
        mat4 InvM = InvT * InvR * InvS;
        
        mat4 MVP = M * camera.GetViewMatrix() * camera.GetProjectionMatrix();
        
        shader->UploadInvM(InvM);
        shader->UploadMVP(MVP);
        shader->UploadM(M);
    }
    
    void UploadAttributes(Shader* shadowShader)
    {
        mat4 T = mat4(
                      1.0, 0.0, 0.0, 0.0,
                      0.0, 1.0, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      position.x, position.y, position.z, 1.0);

        mat4 S = mat4(
                      scaling.x, 0.0, 0.0, 0.0,
                      0.0, scaling.y, 0.0, 0.0,
                      0.0, 0.0, scaling.z, 0.0,
                      0.0, 0.0, 0.0, 1.0);
        
        float alpha = orientation / 180.0 * M_PI;
        
        mat4 R = mat4(
                      cos(alpha), 0.0, sin(alpha), 0.0,
                      0.0, 1.0, 0.0, 0.0,
                      -sin(alpha), 0.0, cos(alpha), 0.0,
                      0.0, 0.0, 0.0, 1.0);
        
        mat4 M = S * R * T;
        
        mat4 VP = camera.GetViewMatrix() * camera.GetProjectionMatrix();
        
        shadowShader->UploadVP(VP);
        shadowShader->UploadM(M);
    }
    
    virtual void aim(float dt)
    {
        if (keyboardState['d'])
        {
            orientation = orientation + 30.0 * dt;
        }
        if (keyboardState['a'])
        {
            orientation = orientation - 30.0 * dt;
        }
    }
    
    virtual void Helicam ()
    {
        camera.SetEye(position - GetAvatar() + vec3(0.0, 1.5, 0.0));
        camera.SetLookAt(camera.GetEyePosition() + GetAvatar() * camera.GetDistance());
    }
    
};

class Bomb : public Object
{
public:
    Bomb(Mesh *m, vec3 pos, vec3 sc, float o) : Object(m, pos, sc, o)
    {
        acceleration = vec3(0, -0.8, 0);
        angularVelocity = 0.6;
    }
    
    void Interact(Object* object)
    {
        OBJECT_TYPE type = object->GetType();
        switch (type) {
            case TIGGER:
                break;
            case BULLET:
                if ((GetPosition() - object->GetPosition()).length() < 0.4)
                    alive = false;
                break;
            case GROUND:
                if (GetPosition().y < object->GetPosition().y)
                {
                    GetPosition().y = object->GetPosition().y;
                    velocity = velocity * (-1.0);
                    angularVelocity = angularVelocity * 0.99;
                }
                break;
        }
    }
    
    void setAcceleration()
    {
        if (keyboardState['g'])
            acceleration.y = acceleration.y - 0.2;
    }
    
    OBJECT_TYPE GetType()
    {
        return BOMB;
    }
};

class Tree : public Object
{
public:
    Tree(Mesh *m, vec3 pos, vec3 sc, float o) : Object(m, pos, sc, o)
    {
        acceleration = vec3(0, -0.6, 0);
        angularVelocity = 0.4;
    }
    
    void Interact(Object* object)
    {
        OBJECT_TYPE type = object->GetType();
        switch (type) {
            case TIGGER:
                break;
            case BULLET:
                if ((GetPosition() - object->GetPosition()).length() < 0.4)
                alive = false;
                break;
            case GROUND:
                if (GetPosition().y < object->GetPosition().y)
                 {
                 GetPosition().y = object->GetPosition().y;
                 velocity = velocity * (-1.0);
                 angularVelocity = angularVelocity * 0.99;
                 }
                break;
        }
    }
    
    void setAcceleration()
    {
        if (keyboardState['g'])
        acceleration.y = acceleration.y - 0.2;
    }
    
    OBJECT_TYPE GetType()
    {
        return TREE;
    }
};

class Tigger : public Object
{
    bool movement = true;
    bool heli = true;
    bool win = false;
    bool lose = false;
    float rotation = 30.0;
    
public:
    Tigger(Mesh *m, vec3 pos, vec3 sc, float o) : Object(m, pos, sc, o)
    {
        velocity = vec3(10.0, 10.0, 10.0);
        acceleration = vec3(0.0, 0.0, -0.6);
        angularAcceleration = 270.0;
        angularVelocity = 2.0;
    }
    
    void UploadAttributes()
    {
        mat4 T = mat4(
                      1.0, 0.0, 0.0, 0.0,
                      0.0, 1.0, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      position.x, position.y, position.z, 1.0);
        
        mat4 InvT = mat4(
                         1.0, 0.0, 0.0, 0.0,
                         0.0, 1.0, 0.0, 0.0,
                         0.0, 0.0, 1.0, 0.0,
                         -position.x, -position.y, -position.z, 1.0);
        
        mat4 S = mat4(
                      scaling.x, 0.0, 0.0, 0.0,
                      0.0, scaling.y, 0.0, 0.0,
                      0.0, 0.0, scaling.z, 0.0,
                      0.0, 0.0, 0.0, 1.0);
        
        mat4 InvS = mat4(
                         1.0 / scaling.x, 0.0, 0.0, 0.0,
                         0.0, 1.0 / scaling.y, 0.0, 0.0,
                         0.0, 0.0, 1.0 / scaling.z, 0.0,
                         0.0, 0.0, 0.0, 1.0);
        
        float alpha = orientation / 180.0 * M_PI;
        
        mat4 R = mat4(
                      cos(alpha), 0.0, sin(alpha), 0.0,
                      0.0, 1.0, 0.0, 0.0,
                      -sin(alpha), 0.0, cos(alpha), 0.0,
                      0.0, 0.0, 0.0, 1.0);
        
        float beta = rotation / 180.0 * M_PI;
        
        mat4 R0 = mat4(cos(beta), sin(beta), 0.0, 0.0,
                       -sin(beta), cos(beta), 0.0, 0.0,
                       0.0, 0.0, 1.0, 0.0,
                       0.0, 0.0, 0.0, 1.0);
        
        mat4 InvR = mat4(
                         cos(alpha), 0.0, -sin(alpha), 0.0,
                         0.0, 1.0, 0.0, 0.0,
                         sin(alpha), 0.0, cos(alpha), 0.0,
                         0.0, 0.0, 0.0, 1.0);
        
        mat4 M;
        
        if (!lose) {
            M = S * R * T;
        } else {
            M = S * R0 * T;
        }
        
        mat4 InvM = InvT * InvR * InvS;
        
        mat4 MVP = M * camera.GetViewMatrix() * camera.GetProjectionMatrix();
        
        shader->UploadInvM(InvM);
        shader->UploadMVP(MVP);
        shader->UploadM(M);
    }
    

    
    void Interact(Object* object)
    {
        OBJECT_TYPE type = object->GetType();
        switch (type) {
            case TREE:
                break;
            case GROUND:
                if (GetPosition().y < object->GetPosition().y)
                {
                    GetPosition().y = object->GetPosition().y;
                    velocity = velocity * (-0.99);
                    angularVelocity = angularVelocity * 0.99;
                }
                break;
        }
    }
    
    void Move(float dt)
    {
        if (win) {
        // update velocity, angular velocity, position and orientation using acceleration and angular acceleration
        velocity = velocity + acceleration * dt;
        position = position + velocity * dt;
        angularVelocity = angularVelocity + angularAcceleration * dt;
        orientation = orientation + angularVelocity * dt;
        }
        
    }
    
    void IntoTheVoid(float dt) {
        if (lose) {
            //velocity = velocity + acceleration * dt;
            //position = position + velocity * dt;
            angularVelocity = angularVelocity + angularAcceleration * dt;
            rotation = rotation + angularVelocity * dt;
            if (scaling.x > 0.0) {
                scaling = scaling + vec3(0.001, 0.001, 0.001) * -1 * 6 * (float)sin(DT);
            }
        }
    }
    
    void setScaling(vec3 sc) {
        scaling = sc;
    }
    
    void setOrientation(float o) {
        orientation = o;
    }
    
    void setPosition(vec3 pos) {
        position = pos;
    }
    
    bool getHeli() {
        return heli;
    }
    
    void setWin(bool b) {
        win = b;
    }
    
    void setLose(bool b) {
        lose = b;
    }
    
    bool hasLost() {
        return lose;
    }
    
    void aim(float dt)
    {
        if (movement) {
            if (keyboardState['d'])
            {
            orientation = orientation + 20.0 * dt;
            }
            if (keyboardState['a'])
            {
            orientation = orientation - 20.0 * dt;
            }
        }
    }
    
    vec3 getPosition() {
        return position;
    }
    
    float getOrientation() {
        return orientation;
    }
    
    void setMovement(bool b) {
        movement = b;
    }
    
    void Helicam ()
    {
        if (heli) {
        camera.SetEye(position - GetAvatar() + vec3(0.0, 1.5, 0.0));
        camera.SetLookAt(camera.GetEyePosition() + GetAvatar() * camera.GetDistance());
        }
    }
    
    void setHeli (bool b) {
        heli = b;
    }
    
    OBJECT_TYPE GetType()
    {
        return TIGGER;
    }
    
};

class Ground : public Object
{
    
public:
    Ground(Mesh *m, vec3 pos, vec3 sc, float o) : Object(m, pos, sc, o)
    {
        
    }
    
    void Interact(Object* object)
    {
        
    }
    
    OBJECT_TYPE GetType()
    {
        return GROUND;
    }
};

Tigger* tigger;

class Bullet : public Object
{
    bool fly = false;
    float rotation = 60.0;
    
public:
    Bullet(Mesh *m, vec3 pos, vec3 sc, float o) : Object(m, pos, sc, o)
    {
        velocity = tigger->GetAvatar() * 0.5;
        acceleration = vec3(0.0, 0.0, -0.6);
        angularAcceleration = 270.0;
        angularVelocity = 2.0;
    }
    
    void Interact(Object* object)
    {
        OBJECT_TYPE type = object->GetType();
        switch (type) {
            case TIGGER:
                break;
            case GROUND:
                break;
            case TREE:
                if ((GetPosition() - object->GetPosition()).length() < 0.4)
                    alive = false;
                break;
            case BOMB:
                if ((GetPosition() - object->GetPosition()).length() < 0.4)
                    alive = false;
                break;
        }
    }
    
    void UploadAttributes()
    {
        mat4 T = mat4(
                      1.0, 0.0, 0.0, 0.0,
                      0.0, 1.0, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      position.x, position.y, position.z, 1.0);
        
        mat4 InvT = mat4(
                         1.0, 0.0, 0.0, 0.0,
                         0.0, 1.0, 0.0, 0.0,
                         0.0, 0.0, 1.0, 0.0,
                         -position.x, -position.y, -position.z, 1.0);
        
        mat4 S = mat4(
                      scaling.x, 0.0, 0.0, 0.0,
                      0.0, scaling.y, 0.0, 0.0,
                      0.0, 0.0, scaling.z, 0.0,
                      0.0, 0.0, 0.0, 1.0);
        
        mat4 InvS = mat4(
                         1.0 / scaling.x, 0.0, 0.0, 0.0,
                         0.0, 1.0 / scaling.y, 0.0, 0.0,
                         0.0, 0.0, 1.0 / scaling.z, 0.0,
                         0.0, 0.0, 0.0, 1.0);
        
        float alpha = orientation / 180.0 * M_PI;
        
        mat4 R = mat4(
                      cos(alpha), 0.0, sin(alpha), 0.0,
                      0.0, 1.0, 0.0, 0.0,
                      -sin(alpha), 0.0, cos(alpha), 0.0,
                      0.0, 0.0, 0.0, 1.0);
        
        float beta = rotation / 180.0 * M_PI;
        
        mat4 R0 = mat4(cos(beta), sin(beta), 0.0, 0.0,
                        -sin(beta), cos(beta), 0.0, 0.0,
                       0.0, 0.0, 1.0, 0.0,
                       0.0, 0.0, 0.0, 1.0);
        
        mat4 InvR = mat4(
                         cos(alpha), 0.0, -sin(alpha), 0.0,
                         0.0, 1.0, 0.0, 0.0,
                         sin(alpha), 0.0, cos(alpha), 0.0,
                         0.0, 0.0, 0.0, 1.0);
        
        mat4 M = S * R * R0 * T;
        mat4 InvM = InvT * InvR * InvS;
        
        mat4 MVP = M * camera.GetViewMatrix() * camera.GetProjectionMatrix();
        
        shader->UploadInvM(InvM);
        shader->UploadMVP(MVP);
        shader->UploadM(M);
    }
    
    
    void updatePosition(Object* obj) {
        vec3 coords = cross(vec3(0.0, 1.0, 0.0), obj->GetAvatar());
        position = vec3(0.0, 0.7, 0.0) + coords * -0.6;
        //position = obj->GetPosition() + obj->GetAvatar() * 0.5;
    }
    
    void updateVelocity(Object* obj) {
        velocity = obj->GetAvatar() * 2.5;
    }
    
    void Fly(float dt)
    {
        if (fly) {
           velocity = velocity + acceleration * dt;
           position = position + velocity * dt;
            angularVelocity = angularVelocity + angularAcceleration * dt;
            rotation = rotation + angularVelocity * dt;
        }
    }
    
    void setFly()
    {
        if (keyboardState['f']) {
            fly = true;
            tigger->setMovement(true);
        }
    }
    
    OBJECT_TYPE GetType()
    {
        return BULLET;
    }
};



Ground* ground;
std::vector<Tree*> trees;
Bullet* bullet;
std::vector<Object*> objects;
std::vector<Bomb*> bombs;
bool life = true;

class Scene
{
    MeshShader *meshShader;
    InfiniteQuadShader *infShader;
    ShadowShader *shadowShader;
    
    std::vector<Texture*> textures;
    std::vector<Material*> materials;
    std::vector<Geometry*> geometries;
    std::vector<Mesh*> meshes;

public:
    Scene()
    {
        meshShader = 0;
        infShader = 0;
        shadowShader = 0;
    }
    
    void Initialize()
    {
        meshShader = new MeshShader();
        infShader = new InfiniteQuadShader();
        shadowShader = new ShadowShader();
        
        vec3 ka = vec3(0.1, 0.1, 0.1);
        vec3 kd = vec3(1.0, 1.0, 1.0);
        vec3 ks = vec3(0.3, 0.3, 0.3);
        
        
        textures.push_back(new Texture("/Users/sanahsuri/Desktop/AIT/Computer Graphics/Tigger/Tigger/Meshes/tigger.png"));
        materials.push_back(new Material(meshShader, textures[0], ka, kd, ks, 50));
        geometries.push_back(new PolygonalMesh("/Users/sanahsuri/Desktop/AIT/Computer Graphics/Tigger/Tigger/Meshes/tigger.obj"));
        meshes.push_back(new Mesh(geometries[0], materials[0]));
        
        textures.push_back(new Texture("/Users/sanahsuri/Desktop/AIT/Computer Graphics/Tigger/Tigger/Meshes/red.png"));
        materials.push_back(new Material(meshShader, textures[1], ka, kd, ks, 50));
        geometries.push_back(new PolygonalMesh("/Users/sanahsuri/Desktop/AIT/Computer Graphics/Tigger/Tigger/Meshes/sphere.obj"));
        meshes.push_back(new Mesh(geometries[1], materials[1]));
        
        // blue texture = 2
        textures.push_back(new Texture("/Users/sanahsuri/Desktop/AIT/Computer Graphics/Tigger/Tigger/Meshes/blue.png"));
        materials.push_back(new Material(meshShader, textures[2], ka, kd, ks, 50));
        geometries.push_back(new PolygonalMesh("/Users/sanahsuri/Desktop/AIT/Computer Graphics/Tigger/Tigger/Meshes/sphere.obj"));
        meshes.push_back(new Mesh(geometries[2], materials[2]));
        
        textures.push_back(new Texture("/Users/sanahsuri/Desktop/AIT/Computer Graphics/Tigger/Tigger/Meshes/yellow.png"));
        materials.push_back(new Material(meshShader, textures[3], ka, kd, ks, 50));
        geometries.push_back(new PolygonalMesh("/Users/sanahsuri/Desktop/AIT/Computer Graphics/Tigger/Tigger/Meshes/sphere.obj"));
        meshes.push_back(new Mesh(geometries[3], materials[3]));
        
        textures.push_back(new Texture("/Users/sanahsuri/Desktop/AIT/Computer Graphics/Tigger/Tigger/Meshes/grass.png"));
        materials.push_back(new Material(infShader, textures[4], ka, kd, ks, 50));
        geometries.push_back(new TexturedQuad);
        meshes.push_back(new Mesh(geometries[4], materials[4]));
        
        textures.push_back(new Texture("/Users/sanahsuri/Desktop/AIT/Computer Graphics/Tigger/Tigger/Meshes/heliait.png"));
        materials.push_back(new Material(meshShader, textures[5], ka, kd, ks, 50));
        geometries.push_back(new PolygonalMesh("/Users/sanahsuri/Desktop/AIT/Computer Graphics/Tigger/Tigger/Meshes/thunderbolt_airscrew.obj"));
        meshes.push_back(new Mesh(geometries[5], materials[5]));
        
        // 6
        textures.push_back(new Texture("/Users/sanahsuri/Desktop/AIT/Computer Graphics/Tigger/Tigger/Meshes/sky.jpg"));
        materials.push_back(new Material(meshShader, textures[6], ka, kd, ks, 50));
        geometries.push_back(new PolygonalMesh("/Users/sanahsuri/Desktop/AIT/Computer Graphics/Tigger/Tigger/Meshes/sphere.obj"));
        meshes.push_back(new Mesh(geometries[6], materials[6]));
        
        
    
        // initial velocity = getahead of avatar * something
        
        // blue balls = 2
        float x = 0.0;
        for (int i = 0; i < 10; i++) {
            Tree* tree_obj = new Tree(meshes[2], vec3(-1.0 + x, 1.0 + x, -4.0), vec3(0.01, 0.01, 0.01), 0.0);
            objects.push_back(tree_obj);
            trees.push_back(tree_obj);
            x += 0.4;
        }
        
        // yellow balls = 3
        float y = 0.0;
        for (int i = 0; i < 5; i++) {
            Tree* tree_obj = new Tree(meshes[3], vec3(1.0 - y, 1.0 - y, -4.0), vec3(0.01, 0.01, 0.01), 0.0);
            objects.push_back(tree_obj);
            trees.push_back(tree_obj);
            y = y - 0.4;
        }
        
        // red balls = 1
        float z;
        for (int i = 0; i < 8; i++) {
            z = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
            Tree* tree_obj = new Tree(meshes[1], vec3(-1.0 * z, 5.0 * z, -4.0), vec3(0.01, 0.01, 0.01), 0.0);
            objects.push_back(tree_obj);
            trees.push_back(tree_obj);
        }
        
        tigger = new Tigger(meshes[0], vec3(0.0, 0.0, 1.7), vec3(0.04, 0.04, 0.04), 93.63);
        objects.push_back(tigger);
        
        Tree* tree_obj0 = new Tree(meshes[1], vec3(-2.0, 1.0, -4.0), vec3(0.01, 0.01, 0.01), -60.0);
        objects.push_back(tree_obj0);
        trees.push_back(tree_obj0);
        
        
        Bomb* bomb = new Bomb(meshes[6], vec3(-1.5, 2.0, -4.0), vec3(0.01, 0.01, 0.01), -60.0);
        objects.push_back(bomb);
        bombs.push_back(bomb);
        
        Bomb* bomb0 = new Bomb(meshes[6], vec3(0.5, 1.0, -4.0), vec3(0.01, 0.01, 0.01), -60.0);
        objects.push_back(bomb0);
        bombs.push_back(bomb0);
        
        Bomb* bomb1 = new Bomb(meshes[6], vec3(-1.0, 1.0, -4.0), vec3(0.01, 0.01, 0.01), -60.0);
        objects.push_back(bomb1);
        bombs.push_back(bomb1);
        
        Bomb* bomb2 = new Bomb(meshes[6], vec3(-2.0, 2.0, -4.0), vec3(0.01, 0.01, 0.01), -60.0);
        objects.push_back(bomb2);
        bombs.push_back(bomb2);
         
        
        //bullet = new Bullet(meshes[5], vec3(1.0, 0.0, -2.0), vec3(0.01, 0.01, 0.01), 0.0);
        
        
        printf("ahead: x = %f, y = %f, z = %f", tigger->GetAvatar().x, tigger->GetAvatar().y, tigger->GetAvatar().z);
        
        ground = new Ground(meshes[4], vec3(-20.0, -1.0, -3.0), vec3(1.0, 1.0, 1.0), 0.0);
        objects.push_back(ground);
        
        // cross prodict of y and getahead
        
        vec3 coords = cross(vec3(0.0, 1.0, 0.0), tigger->GetAvatar());
        bullet = new Bullet(meshes[5], vec3(0.0, 0.5, 0.0) + coords * -0.8, vec3(0.05, 0.05, 0.05), 93.63);
        //objects.push_back(bullet);
        // intial position of the ball with this local coordinate system
        
    }
    
    ~Scene()
    {
        for (int i = 0; i < textures.size(); i++) delete textures[i];
        for (int i = 0; i < materials.size(); i++) delete materials[i];
        for (int i = 0; i < geometries.size(); i++) delete geometries[i];
        for (int i = 0; i < meshes.size(); i++) delete meshes[i];
        for (int i = 0; i < objects.size(); i++) delete objects[i];
        
        if (meshShader) delete meshShader;
    }
    
    void Draw()
    {
        updateObjects();
        onLose();
        onWin();
        for (int i = 0; i < objects.size(); i++) {
            objects[i]->Draw();
            objects[i]->DrawShadow(shadowShader);
        }
    }
    
    void updateObjects()
    {
        std::vector<Object*> updated;
        for (int i = 0; i < objects.size(); i++) {
            if (objects[i]->isAlive()) {
                updated.push_back(objects[i]);
            }
        }
        objects = updated;
    }
    
    void addBullet() {
        if (keyboardState['c']) {
            vec3 coords = cross(vec3(0.0, 1.0, 0.0), tigger->GetAvatar());
            bullet = new Bullet(meshes[5], vec3(0.0, 0.5, 0.0) + coords * -0.8, vec3(0.05, 0.05, 0.05), 0.0);
            objects.push_back(bullet);
            printf("bullet: x = %f, y= %f, z = %f\n", bullet->GetPosition().x, bullet->GetPosition().y, bullet->GetPosition().z);
            tigger->setMovement(false);
        }
    }
    
    void onWin() {
        int count = 0.0;
        for (int i = 0; i < objects.size(); i++) {
            if (objects[i]->GetType() == TREE) {
                count++;
            }
        }
        if (count == 0) {
            tigger->setHeli(false);
            tigger->setPosition(vec3(0.0, 0.0, -3.0));
            tigger->setScaling(vec3(0.1, 0.1, 0.1));
            tigger->setOrientation(-90.0);
            if (life) {
                counter = 100;
            }
            life = false;
        }
    }
    
    void onLose() {
        int count = 0;
        for (int i = 0; i < objects.size(); i++) {
            if (objects[i]->GetType() == BOMB) {
                count++;
            }
        }
        if (count < 4) {
            tigger->setLose(true);
            tigger->setHeli(false);
            tigger->setPosition(vec3(0.0, 1.0, 0.0));
            tigger->setOrientation(-90);
        }
    }

};

Scene scene;

void onInitialization()
{
    glViewport(0, 0, windowWidth, windowHeight);
    
    scene.Initialize();
}

void onExit()
{
    printf("exit");
}

void onDisplay()
{
    
    glClearColor(0, 0, 1.0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    scene.Draw();
    scene.updateObjects();
    
    glutSwapBuffers();
    
}

void onKeyboard(unsigned char key, int x, int y)
{
    keyboardState[key] = true;
    //camera.Quake();
    camera.Control();
    scene.addBullet();
    for (int i = 0; i < trees.size(); i++) {
        trees[i]->setAcceleration();
    }
    bullet->setFly();
}

void onKeyboardUp(unsigned char key, int x, int y)
{
    keyboardState[key] = false;
    //camera.Quake();
    camera.Control();
    scene.addBullet();
    for (int i = 0; i < trees.size(); i++) {
        trees[i]->setAcceleration();
    }
    bullet->setFly();
}

void onReshape(int winWidth, int winHeight)
{
    camera.SetAspectRatio((float)winWidth / winHeight);
    glViewport(0, 0, winWidth, winHeight);
}

void onIdle() {
    double t = glutGet(GLUT_ELAPSED_TIME) * 0.001;
    static double lastTime = 0.0;
    double dt = t - lastTime;
    lastTime = t;
    DT = dt;
    if (counter > 0) {
        counter--;
    }
    
    
    tigger->IntoTheVoid(dt);
    camera.Quake(dt);
    tigger->Move(dt);
    for (int i = 0; i < trees.size(); i++) {
        trees[i]->Move(dt);
        trees[i]->Interact(ground);
        trees[i]->Interact(bullet);
        bullet->Interact(trees[i]);
    }
    
    for (int i = 0; i < bombs.size(); i++) {
        bombs[i]->Move(dt);
        bombs[i]->Interact(ground);
        bombs[i]->Interact(bullet);
        bullet->Interact(bombs[i]);
    }
    
    tigger->aim(dt);
    //bullet->updatePosition(tigger);
    bullet->updateVelocity(tigger);
    bullet->Fly(dt);
    tigger->Helicam();
    tigger->SetLight(spotlight);
    camera.Move(dt);
    
    glutPostRedisplay();
}

int main(int argc, char * argv[])
{
    glutInit(&argc, argv);
#if !defined(__APPLE__)
    glutInitContextVersion(majorVersion, minorVersion);
#endif
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(50, 50);
#if defined(__APPLE__)
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_3_2_CORE_PROFILE);
#else
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
    glutCreateWindow("3D Mesh Rendering");
    
#if !defined(__APPLE__)
    glewExperimental = true;
    glewInit();
#endif
    printf("GL Vendor    : %s\n", glGetString(GL_VENDOR));
    printf("GL Renderer  : %s\n", glGetString(GL_RENDERER));
    printf("GL Version (string)  : %s\n", glGetString(GL_VERSION));
    glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
    printf("GL Version (integer) : %d.%d\n", majorVersion, minorVersion);
    printf("GLSL Version : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    
    onInitialization();
    
    glutDisplayFunc(onDisplay);
    glutIdleFunc(onIdle);
    glutKeyboardFunc(onKeyboard);
    glutKeyboardUpFunc(onKeyboardUp);
    glutReshapeFunc(onReshape);
    
    glutMainLoop();
    onExit();
    return 1;
}
