#pragma once

#include "WW3D2/dx8wrapper.h"

#include <vector>

class MeshClass;
class RenderObjClass;
class SceneClass;

class RetailTerrainShadow : public DX8_CleanupHook
{
public:
	RetailTerrainShadow();
	~RetailTerrainShadow();
	void ReleaseResources() override;
	void ReAcquireResources() override;

	void render(SceneClass &scene, const Vector3 &centre, Int size);
	void castMesh(MeshClass &mesh, const Matrix3D &transform, Bool tree = FALSE);
	bool contains(const Vector3 &position, Real radius) const;
	bool beginTerrainPass();
	void endTerrainPass();
	bool isReady() const { return m_ready; }
	bool getAuthoredShadow(void *&texture, D3DMATRIX &worldToShadow, float &texelSize) const;
	void traceReceiver(bool compiled, bool authored, bool legacy) const;

private:
	struct CasterVertex
	{
		float x, y, depth, rhw;
		float depthU, depthV;
		float u, v;
	};

	bool prepare(Int size);
	void castObject(RenderObjClass &object);
	void project(const Vector3 &position, Vector3 &coord) const;
	void worldToShadowMatrix(D3DMATRIX &matrix) const;

	IDirect3DTexture8 *m_texture = nullptr;
	IDirect3DSurface8 *m_surface = nullptr;
	IDirect3DSurface8 *m_depth = nullptr;
	DWORD m_depthShader = 0;
	DWORD m_cutoutShader = 0;
	DWORD m_receiveShader = 0;
	Int m_size = 0;
	Int m_authoredSize = 0;
	Real m_viewDistance = 1500.0f;
	bool m_ready = false;
	bool m_floatDepth = false;
	Vector3 m_centre;
	Vector3 m_right;
	Vector3 m_up;
	Vector3 m_ray;
	std::vector<CasterVertex> m_vertices;
	std::vector<Vector3> m_deformed;
};

extern RetailTerrainShadow *TheRetailTerrainShadow;
