/*
 * Stellarium Scenery3d Plug-in
 *
 * Copyright (C) 2011 Simon Parzer, Peter Neubauer, Georg Zotti, Andrei Borza
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/** OBJ loader based on dhpoware's glObjViewer (http://www.dhpoware.com/demos/glObjViewer.html) See license below **/

//-----------------------------------------------------------------------------
// Copyright (c) 2007 dhpoware. All Rights Reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#ifndef _OBJ_HPP_
#define _OBJ_HPP_

#include <QFile>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>

#include "StelTexture.hpp"
#include "VecMath.hpp"
#include "AABB.hpp"

class Heightmap;

//! A basic Wavefront .OBJ format model loader.
//!
//! FS: The internal loader still is not very robust, uses many C IO functions (fopen,fscanf...) 
//! and will have serious problems handling a malformed file including many potential buffer overflows. 
//! Meaning: **Do NOT use for untrusted, downloaded files!**
class OBJ
{
public:
    //! OBJ files can have vertices encoded in different order.
    //! Only XYZ and XZY may occur in real life, but we can cope with all...
    enum vertexOrder { XYZ, XZY, YXZ, YZX, ZXY, ZYX };
    //!< Supported OpenGL illumination models. Use specular sparingly!
    enum Illum { DIFFUSE, DIFFUSE_AND_AMBIENT, SPECULAR, TRANSLUCENT=9 };

    struct Material
    {
        Material() {
            ambient[0] = 0.2f;
            ambient[1] = 0.2f;
	    ambient[2] = 0.2f;
            diffuse[0] = 0.8f;
            diffuse[1] = 0.8f;
	    diffuse[2] = 0.8f;
            specular[0] = 0.0f;
            specular[1] = 0.0f;
	    specular[2] = 0.0f;
            emission[0] = 0.0f;
            emission[1] = 0.0f;
	    emission[2] = 0.0f;
	    shininess = 8.0f;
	    name = "<invalid>";
	    alpha = 1.0f;
	    alphatest = false;
	    backfacecull = true;
            illum = DIFFUSE;
        }
        //! Material name
        QString name;
        //! Ka, Kd, Ks, Ke
	QVector3D ambient;
	QVector3D diffuse;
	QVector3D specular;
	QVector3D emission;
        //! Shininess [0..128]
        float shininess;
        //! Transparency [0..1]
        float alpha;
	//! If to perform binary alpha testing. Default off.
	bool alphatest;
	//! If to perform backface culling. Default on.
	bool backfacecull;
        //!< illumination model, copied from MTL.
        Illum illum;
        //! Texture name
        QString textureName;
        //!< Shared pointer to texture of the model. This can be null.
        StelTextureSP texture;
        //! Bump map name
        QString bumpMapName;
        //!< Shared pointer to bump map texture of the model. This can be null.
        StelTextureSP bump_texture;
        //! Height map name
        QString heightMapName;
        //!< Shared pointer to height map texture of the model. This can be null.
	StelTextureSP height_texture;
	//! Name of emissive texture
	QString emissiveMapName;
	//!< Shared pointer to emissive texture of the model. This can be null.
	StelTextureSP emissive_texture;
    };

    //! A vertex struct holds the vertex itself (position), corresponding texture coordinates, normals, tangents and bitangents
    struct Vertex
    {
        Vertex() : position(0.0f), texCoord(0.0f), normal(0.0f), tangent(0.0f), bitangent(0.0f) {}
	Vec3f position;
        Vec2f texCoord;
        Vec3f normal;
        Vec4f tangent;
        Vec3f bitangent;
    };

    //! Structure for a Mesh, will be used with Stellarium to render
    //! Holds the starting index, the number of triangles and a pointer to the MTL
    struct StelModel
    {
        int startIndex, triangleCount;
	//materials are managed by OBJ
        const Material* pMaterial;
	//AABB is managed by this
	AABB bbox;
    };

    //! Initializes values
    OBJ();
    //! Destructor
    ~OBJ();

    //! Cleanup, will be called inside the destructor
    void clean();
    //! Loads the given obj file and, if specified rebuilds normals
    bool load(const QString& filename, const enum vertexOrder order, bool rebuildNormals = false);
    //! Transform all the vertices through multiplication with a 4x4 matrix.
    //! @param mat Matrix to multiply vertices with.
    void transform(QMatrix4x4 mat);
    //! Returns a Material
    Material &getMaterial(int i);
    //! Returns a StelModel
    const StelModel& getStelModel(int i) const;

    //! Getters for various datastructures
    int getNumberOfIndices() const;
    int getNumberOfStelModels() const;
    int getNumberOfTriangles() const;
    int getNumberOfVertices() const;
    int getNumberOfMaterials() const;

    //! Returns a vertex reference
    const Vertex& getVertex(int i) const;
    //! Returns the vertex array
    const Vertex* getVertexArray() const;
    //! Returns the vertex size
    int getVertexSize() const;

    //! Returns flags
    bool isLoaded() const;
    bool hasPositions() const;
    bool hasTextureCoords() const;
    bool hasNormals() const;
    bool hasTangents() const;
    bool hasStelModels() const;

    //! Returns the bounding box for this OBJ
    //const BoundingBox* getBoundingBox() const;
    const AABB& getBoundingBox();

    void renderAABBs();

    //! Returns an estimate of the memory usage of this instance (not fully accurate, but good enough)
    size_t memoryUsage();

    //! Uploads the textures to GL (requires valid context)
    void uploadTexturesGL();
    //! Uploads the vertex and index data to GL buffers (requires valid context)
    void uploadBuffersGL();

    //! Binds the necessary GL objects, making the OBJ ready for drawing. Uses a VAO if the platform supports it.
    void bindGL();
    //! Unbinds this object's GL objects
    void unbindGL();

    //! Legacy fixed-function binding, will be removed when changing to shader-based render
    void bindGLFixedFunction();
    void unbindGLFixedFunction();

    //! Set up some stuff that requires a valid OpenGL context.
    static void setupGL();

    //! Copy assignment operator. No deep copies are performed, but QVectors have copy-on-write semantics, so this is no problem. Does not copy GL objects.
    OBJ& operator=(const OBJ& other);
private:
    typedef QVector<int> IntVector;
    typedef QVector<Vec3f> VF3Vector;
    typedef QVector<Vec2f> VF2Vector;
    typedef Vec3f VPos;
    typedef QVector<Vec3f> PosVector;
    typedef QMap<QString,int> MatCacheT;
    typedef QMap<int, QVector<int> > VertCacheT;

    void addTrianglePos(const PosVector& vertexCoords, IntVector& attributeArray, VertCacheT &vertexCache, unsigned int index, int material, int v0, int v1, int v2);
    void addTrianglePosNormal(const PosVector &vertexCoords,const VF3Vector& normals, IntVector &attributeArray, VertCacheT &vertexCache, unsigned int index, int material,
			      int v0, int v1, int v2,
			      int vn0, int vn1, int vn2);
    void addTrianglePosTexCoord(const PosVector &vertexCoords,const VF2Vector &textureCoords, IntVector &attributeArray, VertCacheT &vertexCache, unsigned int index, int material,
				int v0, int v1, int v2,
				int vt0, int vt1, int vt2);
    void addTrianglePosTexCoordNormal(PosVector &vertexCoords,const VF2Vector &textureCoords,const VF3Vector &normals, IntVector &attributeArray, VertCacheT &vertexCache, unsigned int index, int material,
				      int v0, int v1, int v2,
				      int vt0, int vt1, int vt2,
				      int vn0, int vn1, int vn2);
    int addVertex(VertCacheT &vertexCache, int hash, const Vertex* pVertex);
    //! Builds the StelModels based on material
    void buildStelModels(const IntVector &attributeArray);
    //! Generates normals in case they aren't specified/need rebuild
    void generateNormals();
    //! Generates tangents (and bitangents/binormals) (useful for NormalMapping, Parallax Mapping, ...)
    void generateTangents();
    //! First pass - scans the file for memory allocation
    void importFirstPass(QFile& pFile, MatCacheT &materialCache);
    //! Second pass - actual parsing step
    void importSecondPass(FILE *pFile, const enum vertexOrder order, const MatCacheT &materialCache);
    //! Imports material file and fills the material datastructure
    bool importMaterials(const QString& filename, MatCacheT& materialCache);
    QString absolutePath(QString path);
    //! Determine the bounding box extrema
    void findBounds();
    //! Binds the GL buffers to the vertex attributes
    void bindBuffersGL();
    //! Releases vertex attribute bindings and buffers
    void unbindBuffersGL();

    //! Flags
    bool m_loaded;
    bool m_hasPositions;
    bool m_hasTextureCoords;
    bool m_hasNormals;
    bool m_hasTangents;
    bool m_hasStelModels;
    static bool vertexArraysSupported;

    //! Structure sizes
    unsigned int m_numberOfVertexCoords;
    unsigned int m_numberOfTextureCoords;
    unsigned int m_numberOfNormals;
    unsigned int m_numberOfTriangles;
    unsigned int m_numberOfMaterials;
    unsigned int m_numberOfStelModels;

    //! Bounding box for the entire scene
    AABB pBoundingBox;
    Mat4d m;

    //! Base path to this file
    QString m_basePath;

    //! Datastructures
    QVector<StelModel> m_stelModels;
    QVector<Material> m_materials;
    QVector<Vertex> m_vertexArray;
    QVector<unsigned int> m_indexArray;

    //! OpenGL objects
    QOpenGLBuffer m_vertexBuffer;
    QOpenGLBuffer m_indexBuffer;
    QOpenGLVertexArrayObject* m_vertexArrayObject;

    //! Heightmap
    friend class Heightmap;
};

inline OBJ::Material& OBJ::getMaterial(int i) { return m_materials[i]; }

inline const OBJ::StelModel& OBJ::getStelModel(int i) const { return m_stelModels[i]; }

inline int OBJ::getNumberOfIndices() const { return m_numberOfTriangles * 3; }

inline int OBJ::getNumberOfStelModels() const { return m_numberOfStelModels; }

inline int OBJ::getNumberOfTriangles() const { return m_numberOfTriangles; }

inline int OBJ::getNumberOfVertices() const { return static_cast<int>(m_vertexArray.size()); }

inline int OBJ::getNumberOfMaterials() const {return m_numberOfMaterials; }

inline const OBJ::Vertex& OBJ::getVertex(int i) const { return m_vertexArray[i]; }

inline const OBJ::Vertex* OBJ::getVertexArray() const { return &m_vertexArray[0]; }

inline int OBJ::getVertexSize() const { return static_cast<int>(sizeof(Vertex)); }

inline bool OBJ::isLoaded() const{ return m_loaded; }

inline bool OBJ::hasNormals() const{ return m_hasNormals; }

inline bool OBJ::hasPositions() const { return m_hasPositions; }

inline bool OBJ::hasTangents() const { return m_hasTangents; }

inline bool OBJ::hasTextureCoords() const { return m_hasTextureCoords; }

inline bool OBJ::hasStelModels() const { return m_hasStelModels; }

//inline const OBJ::BoundingBox* OBJ::getBoundingBox() const { return pBoundingBox; }
inline const AABB &OBJ::getBoundingBox() {return pBoundingBox; }

inline QString OBJ::absolutePath(QString path) { return m_basePath + path; }

#endif
