/*******************************************************************************
* CGoGN: Combinatorial and Geometric modeling with Generic N-dimensional Maps  *
* Copyright (C) 2015, IGG Group, ICube, University of Strasbourg, France       *
*                                                                              *
* This library is free software; you can redistribute it and/or modify it      *
* under the terms of the GNU Lesser General Public License as published by the *
* Free Software Foundation; either version 2.1 of the License, or (at your     *
* option) any later version.                                                   *
*                                                                              *
* This library is distributed in the hope that it will be useful, but WITHOUT  *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License  *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU Lesser General Public License     *
* along with this library; if not, write to the Free Software Foundation,      *
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.           *
*                                                                              *
* Web site: http://cgogn.unistra.fr/                                           *
* Contact information: cgogn@unistra.fr                                        *
*                                                                              *
*******************************************************************************/

#ifndef CGOGN_IO_SURFACE_IMPORT_H_
#define CGOGN_IO_SURFACE_IMPORT_H_

#include <Eigen/Dense>

#include <cgogn/io/utils.h>

#include <cgogn/core/utils/numerics.h>
#include <cgogn/core/types/mesh_traits.h>
#include <cgogn/core/functions/attributes.h>
#include <cgogn/core/functions/mesh_ops/face.h>

#include <vector>
#include <fstream>

namespace cgogn
{

namespace io
{

template <typename VEC3>
void import_OFF(CMap2& m, const std::string& filename)
{
	std::vector<uint32> faces_nb_edges;
	std::vector<uint32> faces_vertex_indices;

	std::ifstream fp(filename.c_str(), std::ios::in);

	std::string line;
	line.reserve(512);

	// read OFF header
	getline_safe(fp, line);
	if (line.rfind("OFF") == std::string::npos)
	{
		std::cerr << "File \"" << filename << "\" is not a valid off file." << std::endl;
		return;
	}

	// read number of vertices, edges, faces
	const uint32 nb_vertices = read_uint(fp, line);
	const uint32 nb_faces = read_uint(fp, line);
	/*const uint32 nb_edges_ =*/ read_uint(fp, line);

	auto position = add_attribute<VEC3, CMap2::Vertex>(m, "position");

	// read vertices position
	std::vector<uint32> vertices_id;
	vertices_id.reserve(nb_vertices);

	for (uint32 i = 0; i < nb_vertices; ++i)
	{
		float64 x = read_double(fp, line);
		float64 y = read_double(fp, line);
		float64 z = read_double(fp, line);

		VEC3 pos{x, y, z};

		uint32 vertex_id = m.attribute_containers_[CMap2::Vertex::ORBIT].add_line();
		(*position)[vertex_id] = pos;

		vertices_id.push_back(vertex_id);
	}

	// read faces (vertex indices)
	for (uint32 i = 0u; i < nb_faces ; ++i)
	{
		uint32 n = read_uint(fp, line);
		faces_nb_edges.push_back(n);
		for (uint32 j = 0; j < n; ++j)
		{
			uint32 index = read_uint(fp, line);
			faces_vertex_indices.push_back(vertices_id[index]);
		}
	}

	if (faces_nb_edges.size() == 0u)
		return;

	auto darts_per_vertex = add_attribute<std::vector<Dart>, CMap2::Vertex>(m, "darts_per_vertex");

	uint32 faces_vertex_index = 0;
	std::vector<uint32> vertices_buffer;
	vertices_buffer.reserve(16);

	for (uint32 i = 0, end = faces_nb_edges.size(); i < end; ++i)
	{
		uint32 nbe = faces_nb_edges[i];

		vertices_buffer.clear();
		uint32 prev = std::numeric_limits<uint32>::max();

		for (uint32 j = 0; j < nbe; ++j)
		{
			uint32 idx = faces_vertex_indices[faces_vertex_index++];
			if (idx != prev)
			{
				prev = idx;
				vertices_buffer.push_back(idx);
			}
		}
		if (vertices_buffer.front() == vertices_buffer.back())
			vertices_buffer.pop_back();

		nbe = vertices_buffer.size();
		if (nbe > 2)
		{
			CMap1::Face f = add_face(static_cast<CMap1&>(m), nbe, false);
			Dart d = f.dart;
			for (uint32 j = 0u; j < nbe; ++j)
			{
				const uint32 vertex_index = vertices_buffer[j];
				m.set_embedding<CMap2::Vertex>(d, vertex_index);
				(*darts_per_vertex)[vertex_index].push_back(d);
				d = m.phi1(d);
			}
		}
	}

	bool need_vertex_unicity_check = false;
	uint32 nb_boundary_edges = 0;

	m.foreach_dart([&] (Dart d) -> bool
	{
		if (m.phi2(d) == d)
		{
			uint32 vertex_index = m.embedding(CMap2::Vertex(d));

			std::vector<Dart>& next_vertex_darts = value<std::vector<Dart>>(m, darts_per_vertex, CMap2::Vertex(m.phi1(d)));
			bool phi2_found = false;
			bool first_OK = true;

			for (auto it = next_vertex_darts.begin();
				 it != next_vertex_darts.end() && !phi2_found;
				 ++it)
			{
				if (m.embedding(CMap2::Vertex(m.phi1(*it))) == vertex_index)
				{
					if (m.phi2(*it) == *it)
					{
						m.phi2_sew(d, *it);
						phi2_found = true;
					}
					else
						first_OK = false;
				}
			}

			if (!phi2_found)
				++nb_boundary_edges;

			if (!first_OK)
				need_vertex_unicity_check = true;
		}
		return true;
	});

	if (nb_boundary_edges > 0)
	{
//		uint32 nb_holes = mbuild_.close_map();
//		std::cout << nb_holes << " hole(s) have been closed" << std::endl;;
		std::cout << nb_boundary_edges << " boundary edges" << std::endl;
	}

//		if (need_vertex_unicity_check)
//		{
//			map_.template enforce_unique_orbit_embedding<Vertex::ORBIT>();
//			cgogn_log_warning("create_map") << "Import Surface: non manifold vertices detected and corrected";
//		}

	delete darts_per_vertex;

//		cgogn_assert(map_.template is_well_embedded<Vertex>());
//		if (map_.template is_embedded<Face::ORBIT>())
//		{
//			cgogn_assert(map_.template is_well_embedded<Face>());
//		}
//	}
}

} // namespace io

} // namespace cgogn

#endif // CGOGN_IO_SURFACE_IMPORT_H_
