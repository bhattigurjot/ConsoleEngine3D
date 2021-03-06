#include "olcConsoleGameEngine.h"

#include <fstream>
#include <strstream>
#include <algorithm>

struct vec3d
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 1.0f;
};

struct triangle
{
	vec3d p[3];

	wchar_t sym;
	short col;
};

struct mesh
{
	std::vector<triangle> tris;

	bool LoadFromObjFile(std::string _fileName)
	{
		std::fstream f(_fileName);
		if (!f.is_open())
		{
			// File cannot be found
			return false;
		}
		// vertices
		std::vector<vec3d> verts;

		while (!f.eof())
		{
			char line[128];
			f.getline(line, 128);

			std::strstream s;
			s << line;

			char firstChar;

			if (line[0] == 'v')
			{
				vec3d v;
				s >> firstChar >> v.x >> v.y >> v.z;
				verts.push_back(v);
			}

			if (line[0] == 'f')
			{
				int f[3];
				s >> firstChar >> f[0] >> f[1] >> f[2];
				tris.push_back({verts[f[0] - 1], verts[f[1] - 1], verts[f[2] - 1]});
			}
		}

		return true;
	}
};

struct mat4x4
{
	float m[4][4] = { 0 };
};

class ConsoleEngine3D : public olcConsoleGameEngine
{
public:
	ConsoleEngine3D()
	{
		m_sAppName = L"Console Engine 3D";
	}

private:
	mesh meshCube;
	mat4x4 matProj;
	vec3d vCamera;
	vec3d vLookDir; // unit direction 

	float fYaw = 0.0f; // rotation in y-axis

	float fTheta = 0.0f;

	vec3d MatrixMultiplyVector(mat4x4 &m, vec3d &i)
	{
		vec3d v;
		v.x = i.x * m.m[0][0] + i.y * m.m[1][0] + i.z * m.m[2][0] + i.w * m.m[3][0];
		v.y = i.x * m.m[0][1] + i.y * m.m[1][1] + i.z * m.m[2][1] + i.w * m.m[3][1];
		v.z = i.x * m.m[0][2] + i.y * m.m[1][2] + i.z * m.m[2][2] + i.w * m.m[3][2];
		v.w = i.x * m.m[0][3] + i.y * m.m[1][3] + i.z * m.m[2][3] + i.w * m.m[3][3];
		return v;
	}

	mat4x4 MatrixMakeIdentity()
	{
		mat4x4 matrix;
		matrix.m[0][0] = 1.0f;
		matrix.m[1][1] = 1.0f;
		matrix.m[2][2] = 1.0f;
		matrix.m[3][3] = 1.0f;
		return matrix;
	}

	mat4x4 MatrixMakeRotationX(float fAngleRad)
	{
		mat4x4 matrix;
		matrix.m[0][0] = 1.0f;
		matrix.m[1][1] = cosf(fAngleRad);
		matrix.m[1][2] = sinf(fAngleRad);
		matrix.m[2][1] = -sinf(fAngleRad);
		matrix.m[2][2] = cosf(fAngleRad);
		matrix.m[3][3] = 1.0f;
		return matrix;
	}

	mat4x4 MatrixMakeRotationY(float fAngleRad)
	{
		mat4x4 matrix;
		matrix.m[0][0] = cosf(fAngleRad);
		matrix.m[0][2] = sinf(fAngleRad);
		matrix.m[2][0] = -sinf(fAngleRad);
		matrix.m[1][1] = 1.0f;
		matrix.m[2][2] = cosf(fAngleRad);
		matrix.m[3][3] = 1.0f;
		return matrix;
	}

	mat4x4 MatrixMakeRotationZ(float fAngleRad)
	{
		mat4x4 matrix;
		matrix.m[0][0] = cosf(fAngleRad);
		matrix.m[0][1] = sinf(fAngleRad);
		matrix.m[1][0] = -sinf(fAngleRad);
		matrix.m[1][1] = cosf(fAngleRad);
		matrix.m[2][2] = 1.0f;
		matrix.m[3][3] = 1.0f;
		return matrix;
	}

	mat4x4 MatrixMakeTranslation(float x, float y, float z)
	{
		mat4x4 matrix;
		matrix.m[0][0] = 1.0f;
		matrix.m[1][1] = 1.0f;
		matrix.m[2][2] = 1.0f;
		matrix.m[3][3] = 1.0f;
		matrix.m[3][0] = x;
		matrix.m[3][1] = y;
		matrix.m[3][2] = z;
		return matrix;
	}

	mat4x4 MatrixMakeProjection(float fFovDeg, float fAspectRatio, float fNear, float fFar)
	{
		// Projection matrix
		float fFovRad = 1.0f / tanf(fFovDeg * 0.5f / 180.0f * 3.14159f);
		mat4x4 matrix;
		matrix.m[0][0] = fAspectRatio * fFovRad;
		matrix.m[1][1] = fFovRad;
		matrix.m[2][2] = fFar / (fFar - fNear);
		matrix.m[2][3] = (-fNear * fFar) / (fFar - fNear);
		matrix.m[3][2] = 1.0f;
		matrix.m[3][3] = 0.0f;
		return matrix;
	}

	mat4x4 MatrixMultiplyMatrix(mat4x4 &m1, mat4x4 &m2)
	{
		mat4x4 matrix;
		for (int c = 0; c < 4; c++)
		{
			for (int r = 0; r < 4; r++)
			{
				matrix.m[r][c] = m1.m[r][0] * m2.m[0][c] + m1.m[r][1] * m2.m[1][c] + m1.m[r][2] * m2.m[2][c] + m1.m[r][3] * m2.m[3][c];
			}
		}
		return matrix;
	}

	// position - obj should be
	// target - forward vector for that object 
	// up - up vector
	mat4x4 MatrixPointAt(vec3d &pos, vec3d &target, vec3d &up)
	{
		// new forward vector
		vec3d newForward = VectorSubtract(target, pos);
		newForward = VectorNormalize(newForward);

		// new up vector
		vec3d a = VectorMultiply(newForward, VectorDotProduct(up, newForward));
		vec3d newUp = VectorSubtract(up, a);
		newUp = VectorNormalize(newUp);

		// new right direction
		vec3d newRight = VectorCrossProduct(newUp, newForward);

		// create point-at matrix
		mat4x4 matrix;
		matrix.m[0][0] = newRight.x;
		matrix.m[0][1] = newRight.y;
		matrix.m[0][2] = newRight.z;
		matrix.m[0][3] = 0.0f;
		matrix.m[1][0] = newUp.x;
		matrix.m[1][1] = newUp.y;
		matrix.m[1][2] = newUp.z;
		matrix.m[1][3] = 0.0f;
		matrix.m[2][0] = newForward.x;
		matrix.m[2][1] = newForward.y;
		matrix.m[2][2] = newForward.z;
		matrix.m[2][3] = 0.0f;
		matrix.m[3][0] = pos.x;
		matrix.m[3][1] = pos.y;
		matrix.m[3][2] = pos.z;
		matrix.m[3][3] = 1.0f;

		return matrix;
	}

	// for rotation and translation only
	mat4x4 MatrixQuickInverse(mat4x4 &m)
	{
		// create look-at matrix
		mat4x4 matrix;
		matrix.m[0][0] = m.m[0][0];
		matrix.m[0][1] = m.m[1][0];
		matrix.m[0][2] = m.m[2][0];
		matrix.m[0][3] = 0.0f;
		matrix.m[1][0] = m.m[0][1];
		matrix.m[1][1] = m.m[1][1];
		matrix.m[1][2] = m.m[2][1];
		matrix.m[1][3] = 0.0f;
		matrix.m[2][0] = m.m[0][2];
		matrix.m[2][1] = m.m[1][2];
		matrix.m[2][2] = m.m[2][2];
		matrix.m[2][3] = 0.0f;
		matrix.m[3][0] = -(m.m[3][0] * matrix.m[0][0] + m.m[3][1] * matrix.m[1][0] + m.m[3][2] * matrix.m[2][0]);
		matrix.m[3][1] = -(m.m[3][0] * matrix.m[0][1] + m.m[3][1] * matrix.m[1][1] + m.m[3][2] * matrix.m[2][1]);
		matrix.m[3][2] = -(m.m[3][0] * matrix.m[0][2] + m.m[3][1] * matrix.m[1][2] + m.m[3][2] * matrix.m[2][2]);
		matrix.m[3][3] = 1.0f;

		return matrix;
	}

	vec3d VectorAdd(vec3d & v1, vec3d & v2)
	{
		return { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
	}

	vec3d VectorSubtract(vec3d & v1, vec3d & v2)
	{
		return { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
	}

	vec3d VectorMultiply(vec3d & v1, float s)
	{
		return { v1.x * s, v1.y * s, v1.z * s };
	}

	vec3d VectorDivide(vec3d & v1, float s)
	{
		return { v1.x / s, v1.y / s, v1.z / s };
	}

	float VectorDotProduct(vec3d & v1, vec3d & v2)
	{
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}

	float VectorLength(vec3d &v)
	{
		return sqrtf(VectorDotProduct(v, v));
	}

	vec3d VectorNormalize(vec3d & v)
	{
		float l = VectorLength(v);
		return { v.x/l, v.y/l, v.z/l};
	}

	vec3d VectorCrossProduct(vec3d &v1, vec3d &v2)
	{
		vec3d v;
		v.x = v1.y * v2.z - v1.z * v2.y;
		v.y = v1.z * v2.x - v1.x * v2.z;
		v.z = v1.x * v2.y - v1.y * v2.x;
		return v;
	}

	// Return vector if line intersect plane
	vec3d VectorIntersectPlane(vec3d &plane_p, vec3d &plane_n, vec3d &lineStart, vec3d& lineEnd)
	{
		plane_n = VectorNormalize(plane_n);
		float plane_d = VectorDotProduct(plane_n, plane_p);
		float ad = VectorDotProduct(lineStart, plane_n);
		float bd = VectorDotProduct(lineEnd, plane_n);
		float t = (-plane_d - ad) / (bd - ad);
		vec3d lineStartToEnd = VectorSubtract(lineEnd, lineStart);
		vec3d lineToIntersect = VectorMultiply(lineStartToEnd, t);
		return VectorAdd(lineStart, lineToIntersect);
	}

	int TriangleClipAgainstPlain(vec3d plane_p, vec3d plane_n, triangle &in_tri, triangle &out_tri1, triangle &out_tri2)
	{
		// making sure plane is normal
		plane_n = VectorNormalize(plane_n);

		// return signed short distance from point to plane
		auto dist = [&](vec3d &p) {
			// plane normal must be normal
			vec3d n = VectorNormalize(p);
			return (plane_n.x*p.x + plane_n.y*p.y + plane_n.z*p.z - VectorDotProduct(plane_n, plane_p)); // plane equation
		};
		// if dist is + , point is inside of plane

		// create temp storage
		vec3d* inside_points[3]; 
		vec3d* outside_points[3];
		int nInsidePointCount = 0;
		int nOutsidePointCount = 0;

		// get signed distance of each point in triangle to plane
		float d0 = dist(in_tri.p[0]);
		float d1 = dist(in_tri.p[1]);
		float d2 = dist(in_tri.p[2]);

		if (d0 >= 0) {
			inside_points[nInsidePointCount++] = &in_tri.p[0];
		} else {
			outside_points[nOutsidePointCount++] = &in_tri.p[0];
		}
		if (d1 >= 0) {
			inside_points[nInsidePointCount++] = &in_tri.p[1];
		}
		else {
			outside_points[nOutsidePointCount++] = &in_tri.p[1];
		} 
		if (d2 >= 0) {
			inside_points[nInsidePointCount++] = &in_tri.p[2];
		}
		else {
			outside_points[nOutsidePointCount++] = &in_tri.p[2];
		}

		if (nInsidePointCount == 0)
		{
			return 0; // no returned triangles are valid
		}

		if (nInsidePointCount == 3)
		{
			out_tri1 = in_tri;
			return 1; // just one triangle is valid
		}

		if (nInsidePointCount == 1 && nOutsidePointCount == 2)
		{
			out_tri1.col = in_tri.col;
			out_tri1.sym = in_tri.sym;

			out_tri1.p[0] = *inside_points[0];

			out_tri1.p[1] = VectorIntersectPlane(plane_p, plane_n, *inside_points[0], *outside_points[0]);
			out_tri1.p[2] = VectorIntersectPlane(plane_p, plane_n, *inside_points[0], *outside_points[1]);

			return 1; // return one newly formed triangle
		}

		if (nInsidePointCount == 2 && nOutsidePointCount == 1)
		{
			out_tri1.col = in_tri.col;
			out_tri1.sym = in_tri.sym;

			out_tri2.col = in_tri.col;
			out_tri2.sym = in_tri.sym;

			out_tri1.p[0] = *inside_points[0];
			out_tri1.p[1] = *inside_points[1];
			out_tri1.p[2] = VectorIntersectPlane(plane_p, plane_n, *inside_points[0], *outside_points[0]);

			out_tri1.p[0] = *inside_points[1];
			out_tri1.p[1] = out_tri1.p[2];
			out_tri1.p[2] = VectorIntersectPlane(plane_p, plane_n, *inside_points[1], *outside_points[0]);

			return 2; // return two newly formed triangle
		}
	}

public:
	bool OnUserCreate() override
	{
		//meshCube.LoadFromObjFile("VideoShip.obj");
		meshCube.LoadFromObjFile("teapot.obj");
		//meshCube.LoadFromObjFile("axis.obj");

		// Projection matrix
		float fNear = 0.1f;
		float fFar = 1000.0f;
		float fFov = 90.0f; // 90 degrees
		float fAspectRatio = (float)ScreenHeight() / (float)ScreenWidth();
		//float fAspectRatio = (float)ScreenWidth() / (float)ScreenHeight();

		matProj = MatrixMakeProjection(fFov, fAspectRatio, fNear, fFar);

		return true;
	}

	// Taken From Command Line Webcam Video
	CHAR_INFO GetColour(float lum)
	{
		short bg_col, fg_col;
		wchar_t sym;
		int pixel_bw = (int)(13.0f*lum);
		switch (pixel_bw)
		{
		case 0: bg_col = BG_BLACK; fg_col = FG_BLACK; sym = PIXEL_SOLID; break;

		case 1: bg_col = BG_BLACK; fg_col = FG_DARK_GREY; sym = PIXEL_QUARTER; break;
		case 2: bg_col = BG_BLACK; fg_col = FG_DARK_GREY; sym = PIXEL_HALF; break;
		case 3: bg_col = BG_BLACK; fg_col = FG_DARK_GREY; sym = PIXEL_THREEQUARTERS; break;
		case 4: bg_col = BG_BLACK; fg_col = FG_DARK_GREY; sym = PIXEL_SOLID; break;

		case 5: bg_col = BG_DARK_GREY; fg_col = FG_GREY; sym = PIXEL_QUARTER; break;
		case 6: bg_col = BG_DARK_GREY; fg_col = FG_GREY; sym = PIXEL_HALF; break;
		case 7: bg_col = BG_DARK_GREY; fg_col = FG_GREY; sym = PIXEL_THREEQUARTERS; break;
		case 8: bg_col = BG_DARK_GREY; fg_col = FG_GREY; sym = PIXEL_SOLID; break;

		case 9:  bg_col = BG_GREY; fg_col = FG_WHITE; sym = PIXEL_QUARTER; break;
		case 10: bg_col = BG_GREY; fg_col = FG_WHITE; sym = PIXEL_HALF; break;
		case 11: bg_col = BG_GREY; fg_col = FG_WHITE; sym = PIXEL_THREEQUARTERS; break;
		case 12: bg_col = BG_GREY; fg_col = FG_WHITE; sym = PIXEL_SOLID; break;
		default:
			bg_col = BG_BLACK; fg_col = FG_BLACK; sym = PIXEL_SOLID;
		}

		CHAR_INFO c;
		c.Attributes = bg_col | fg_col;
		c.Char.UnicodeChar = sym;
		return c;
	}

	bool OnUserUpdate(float _elapsedTime) override
	{
		vLookDir = { 0.0f, 0.0f, 0.0f };

		// key press movement
		if (GetKey(VK_UP).bHeld)
			vCamera.y += 8.0f * _elapsedTime;
		if (GetKey(VK_DOWN).bHeld)
			vCamera.y -= 8.0f * _elapsedTime;
		if (GetKey(VK_LEFT).bHeld)
			vCamera.x -= 8.0f * _elapsedTime;
		if (GetKey(VK_RIGHT).bHeld)
			vCamera.x += 8.0f * _elapsedTime;

		vec3d vForward = VectorMultiply(vLookDir, 8.0f * _elapsedTime); // Forward velocity vector

		if (GetKey(L'A').bHeld)
			fYaw -= 2.0f * _elapsedTime;
		if (GetKey(L'D').bHeld)
			fYaw += 2.0f * _elapsedTime;
		
		if (GetKey(L'W').bHeld)
			vCamera = VectorAdd(vCamera, vForward);
		if (GetKey(L'S').bHeld)
			vCamera = VectorSubtract(vCamera, vForward);

		mat4x4 matRotZ, matRotX;
		//fTheta += 1.0f * _elapsedTime;

		// Rotation Z
		matRotZ = MatrixMakeRotationZ(fTheta * 0.5f);
		// Rotation X
		matRotX = MatrixMakeRotationX(fTheta);

		mat4x4 matTrans;
		matTrans = MatrixMakeTranslation(0.0f, 0.0f, 5.0f);

		mat4x4 matWorld;
		matWorld = MatrixMakeIdentity();
		matWorld = MatrixMultiplyMatrix(matRotZ, matRotX);
		matWorld = MatrixMultiplyMatrix(matWorld, matTrans);

		// Up vector
		vec3d vUp = { 0.0f, 1.0f, 0.0f };
		//vec3d vTarget = VectorAdd(vCamera, vLookDir);
		vec3d vTarget = { 0.0f,0.0f,1.0f };
		mat4x4 matCameraRotation = MatrixMakeRotationY(fYaw);
		vLookDir = MatrixMultiplyVector(matCameraRotation, vTarget);
		vTarget = VectorAdd(vCamera, vLookDir);

		mat4x4 matCamera = MatrixPointAt(vCamera, vTarget, vUp);
		// make view matrix from camera
		mat4x4 matView = MatrixQuickInverse(matCamera);

		std::vector<triangle> vecTrianglesToRaster;

		// Draw Triangles
		for (auto tri : meshCube.tris)
		{
			triangle triProjected, triTransformed, triViewed;
			
			// Transformations
			triTransformed.p[0] = MatrixMultiplyVector(matWorld, tri.p[0]);
			triTransformed.p[1] = MatrixMultiplyVector(matWorld, tri.p[1]);
			triTransformed.p[2] = MatrixMultiplyVector(matWorld, tri.p[2]);

			vec3d normal, line1, line2;
			// Get lines either side of triangle
			line1 = VectorSubtract(triTransformed.p[1], triTransformed.p[0]);
			line2 = VectorSubtract(triTransformed.p[2], triTransformed.p[0]);

			// Normal
			normal = VectorCrossProduct(line1, line2);
			// length of Normal vector, create unit normal vector
			normal = VectorNormalize(normal);

			// Ray from triangle to camera
			vec3d vCameraRay = VectorSubtract(triTransformed.p[0], vCamera);

			if (VectorDotProduct(normal, vCameraRay) < 0.0f)
			{
				// Illumination & its unit vector
				vec3d lightDirection = { 0.0f, 1.0f, -1.0f };
				lightDirection = VectorNormalize(lightDirection);

				// Dot product b/w normal and light vector
				float dp = max(0.1f, VectorDotProduct(lightDirection, normal));

				CHAR_INFO c = GetColour(dp);
				triTransformed.col = c.Attributes;
				triTransformed.sym = c.Char.UnicodeChar;

				// Convert world space to view space
				triViewed.p[0] = MatrixMultiplyVector(matView, triTransformed.p[0]);
				triViewed.p[1] = MatrixMultiplyVector(matView, triTransformed.p[1]);
				triViewed.p[2] = MatrixMultiplyVector(matView, triTransformed.p[2]);
				triViewed.sym = triTransformed.sym;
				triViewed.col = triTransformed.col;

				int nClippedTriangles = 0;
				triangle clipped[2];
				nClippedTriangles = TriangleClipAgainstPlain({ 0.0f,0.0f,0.1f }, {0.0f,0.0f,1.0f}, triViewed, clipped[0], clipped[1]);

				for (int n = 0; n < nClippedTriangles; n++)
				{

					// Project 3d to 2d (old -  i.e world space to screen space)
					triProjected.p[0] = MatrixMultiplyVector(matWorld, clipped[n].p[0]);
					triProjected.p[1] = MatrixMultiplyVector(matWorld, clipped[n].p[1]);
					triProjected.p[2] = MatrixMultiplyVector(matWorld, clipped[n].p[2]);
					triProjected.col = clipped[n].col;
					triProjected.sym = clipped[n].sym;

					// Scale into view
					// Normalize
					triProjected.p[0] = VectorDivide(triProjected.p[0], triProjected.p[0].w);
					triProjected.p[1] = VectorDivide(triProjected.p[1], triProjected.p[1].w);
					triProjected.p[2] = VectorDivide(triProjected.p[2], triProjected.p[2].w);

					// X/Y are inverted so put them back
					triProjected.p[0].x *= -1.0f;
					triProjected.p[1].x *= -1.0f;
					triProjected.p[2].x *= -1.0f;
					triProjected.p[0].y *= -1.0f;
					triProjected.p[1].y *= -1.0f;
					triProjected.p[2].y *= -1.0f;

					vec3d vOffset = { 1.0f, 1.0f, 0.0f };
					triProjected.p[0] = VectorAdd(triProjected.p[0], vOffset);
					triProjected.p[1] = VectorAdd(triProjected.p[1], vOffset);
					triProjected.p[2] = VectorAdd(triProjected.p[2], vOffset);

					triProjected.p[0].x *= 0.5f * (float)ScreenWidth();
					triProjected.p[0].y *= 0.5f * (float)ScreenHeight();
					triProjected.p[1].x *= 0.5f * (float)ScreenWidth();
					triProjected.p[1].y *= 0.5f * (float)ScreenHeight();
					triProjected.p[2].x *= 0.5f * (float)ScreenWidth();
					triProjected.p[2].y *= 0.5f * (float)ScreenHeight();

					// Add to vetor of triangles to raster
					vecTrianglesToRaster.push_back(triProjected);

				}
				/* Rasterize triangle
				 DrawTriangle() => for wirefame 
				FillTriangle(
					triProjected.p[0].x, triProjected.p[0].y,
					triProjected.p[1].x, triProjected.p[1].y,
					triProjected.p[2].x, triProjected.p[2].y,
					triProjected.sym, triProjected.col
				);

				DrawTriangle(
					triProjected.p[0].x, triProjected.p[0].y,
					triProjected.p[1].x, triProjected.p[1].y,
					triProjected.p[2].x, triProjected.p[2].y,
					PIXEL_SOLID, FG_BLACK
				);*/
			}
		}

		// Sort triangles from back to front - painter's algorithm
		std::sort(vecTrianglesToRaster.begin(), vecTrianglesToRaster.end(), [](triangle &t1, triangle &t2) {
			// mid point
			float z1 = (t1.p[0].z + t1.p[1].z + t1.p[2].z) / 3.0f;
			float z2 = (t2.p[0].z + t2.p[1].z + t2.p[2].z) / 3.0f;
			return z1 > z2;
		});

		// Clear the screen
		Fill(0, 0, ScreenWidth(), ScreenHeight(), PIXEL_SOLID, FG_BLACK);

		// Draw
		//for (auto &tri : vecTrianglesToRaster)
		//{
		//	// Rasterize triangle
		//	// DrawTriangle() => for wirefame 
		//	FillTriangle(
		//		tri.p[0].x, tri.p[0].y,
		//		tri.p[1].x, tri.p[1].y,
		//		tri.p[2].x, tri.p[2].y,
		//		tri.sym, tri.col
		//	);

		//	DrawTriangle(
		//		tri.p[0].x, tri.p[0].y,
		//		tri.p[1].x, tri.p[1].y,
		//		tri.p[2].x, tri.p[2].y,
		//		PIXEL_SOLID, FG_BLACK
		//	);
		//}

		for (auto &triToRaster : vecTrianglesToRaster)
		{
			triangle clipped[2];
			std::list<triangle> listTriangles;
			listTriangles.push_back(triToRaster);
			int nNewTriangles = 1;

			for (int p = 0; p < 4; p++)
			{
				int nTrisToAdd = 0;
				while (nNewTriangles > 0)
				{
					triangle test = listTriangles.front();
					listTriangles.pop_front();
					nNewTriangles--;

					switch (p)
					{
					case 0: nTrisToAdd = TriangleClipAgainstPlain({ 0.0f,0.0f,0.0f }, { 0.0f,1.0f,0.0f }, test, clipped[0], clipped[1]); break;
					case 1: nTrisToAdd = TriangleClipAgainstPlain({ 0.0f,(float)ScreenHeight()-1,0.0f }, { 0.0f,-1.0f,0.0f }, test, clipped[0], clipped[1]); break;
					case 2: nTrisToAdd = TriangleClipAgainstPlain({ 0.0f,0.0f,0.0f }, { 1.0f,0.0f,0.0f }, test, clipped[0], clipped[1]); break;
					case 3: nTrisToAdd = TriangleClipAgainstPlain({ (float)ScreenWidth()-1,0.0f,0.0f }, { -1.0f,0.0f,0.0f }, test, clipped[0], clipped[1]); break;
					default:
						break;
					}

					for (int w = 0; w < nTrisToAdd; w++)
					{
						listTriangles.push_back(clipped[w]);
					}
				}
				nNewTriangles = listTriangles.size();
			}

			for (auto &tri : listTriangles)
			{
				FillTriangle(
					tri.p[0].x, tri.p[0].y,
					tri.p[1].x, tri.p[1].y,
					tri.p[2].x, tri.p[2].y,
					tri.sym, tri.col
				);

				DrawTriangle(
					tri.p[0].x, tri.p[0].y,
					tri.p[1].x, tri.p[1].y,
					tri.p[2].x, tri.p[2].y,
					PIXEL_SOLID, FG_BLACK
				);
			}
		}

		return true;
	}
};

int main()
{
	ConsoleEngine3D demo;
	if (demo.ConstructConsole(256, 240, 4, 4))
	{
		demo.Start();
	}

    return 0;
}

