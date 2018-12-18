#include "ThicknessChecker.h"

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/mesh_segmentation.h>
#include <CGAL/property_map.h>
#include <iostream>
#include <fstream>

#include "Eigen/Core"
#include "igl/copyleft/cgal/mesh_to_polyhedron.h"

typedef CGAL::Exact_predicates_inexact_constructions_kernel Kernel;
typedef CGAL::Polyhedron_3<Kernel> Polyhedron;

////
// originally from: https://doc.cgal.org/latest/Surface_mesh_segmentation/index.html

bool CGAL_SDF(
	const Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>& V,
	const Eigen::Matrix<   int, Eigen::Dynamic, Eigen::Dynamic>& F,
	Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>& F_RAWSDF,
	Eigen::Matrix<   int, Eigen::Dynamic, Eigen::Dynamic>& F_Segment)
{
	try {
		// create Polyhedron
		Polyhedron mesh;
		igl::copyleft::cgal::mesh_to_polyhedron(V, F, mesh);

		// create a property-map for SDF values
		typedef std::map<Polyhedron::Facet_const_handle, double> Facet_double_map;
		Facet_double_map internal_sdf_map;
		boost::associative_property_map<Facet_double_map> sdf_property_map(internal_sdf_map);

		// compute SDF values using default parameters for number of rays, and cone angle
		const std::size_t number_of_rays = 25;  // cast 25 rays per facet
		const double cone_angle = 2.0 / 3.0 * CGAL_PI; // set cone opening-angle
		CGAL::sdf_values(mesh, sdf_property_map, cone_angle, number_of_rays, false);
		std::pair<double, double> min_max_sdf =
			CGAL::sdf_values_postprocessing(mesh, sdf_property_map);

		// create a property-map for segment-ids
		typedef std::map<Polyhedron::Facet_const_handle, std::size_t> Facet_int_map;
		Facet_int_map internal_segment_map;
		boost::associative_property_map<Facet_int_map> segment_property_map(internal_segment_map);

		// segment the mesh using default parameters for number of levels, and smoothing lambda
		// Any other scalar values can be used instead of using SDF values computed using the CGAL function
		std::size_t number_of_segments = CGAL::segmentation_from_sdf_values(mesh, sdf_property_map, segment_property_map);

		const std::size_t number_of_clusters = 4;       // use 4 clusters in soft clustering
		const double smoothing_lambda = 0.3;  // importance of surface features, suggested to be in-between [0,1]
		// Note that we can use the same SDF values (sdf_property_map) over and over again for segmentation.
		// This feature is relevant for segmenting the mesh several times with different parameters.
		CGAL::segmentation_from_sdf_values(
			mesh, sdf_property_map, segment_property_map, number_of_clusters, smoothing_lambda);

		////
		// store F_SDF and F_Segment
		F_RAWSDF.resize(F.rows(), 1);
		F_Segment.resize(F.rows(), 1);
		int fIdx = 0;
		for (Polyhedron::Facet_const_iterator facet_it = mesh.facets_begin(); facet_it != mesh.facets_end(); ++facet_it) {
			F_RAWSDF(fIdx, 0) = min_max_sdf.first + (min_max_sdf.second - min_max_sdf.first) * sdf_property_map[facet_it];
			F_Segment(fIdx, 0) = segment_property_map[facet_it];
			++fIdx;
		}
	}
	catch (int e)
	{
		return false;
	}
	return true;
}