/*
 *  analysis2d_cloth_static.cpp
 *  sensitive couture
 *
 *  Created by Nobuyuki Umetani on 9/14/10.
 *  Copyright 2010 The University of Tokyo and Columbia University. All rights reserved.
 *
 */

#if defined(__VISUALC__)
#pragma warning ( disable : 4996 )
#pragma warning ( disable : 4786 )
#endif

#if defined(__APPLE__) && defined(__MACH__)
#include <GLUT/glut.h>
#else
#include <GL/glew.h>
#include <GL/glut.h>
#endif

#include <memory>

#include "delfem/femls/zsolver_ls_iter.h"
#include "delfem/field_world.h"
#include "delfem/field.h"
#include "delfem/drawer_field.h"
#include "delfem/drawer_field_face.h"
#include "delfem/drawer_field_edge.h"
#include "delfem/ls/solver_ls_iter.h"
#include "delfem/mesh3d.h"
#include "delfem/mesher2d.h"

#include "eqn_glue.h"
#include "cloth_utility.h"
#include "analysis2d_cloth_static.h"
#include "eqn_contact3d.h"

#include "svgpp\SvgManager.h"
#include "svgpp\SvgPolyPath.h"

using namespace Fem::Field;

static unsigned int MakeHingeField_Tri(Fem::Field::CFieldWorld& world, unsigned int id_field_base);

CAnalysis2D_Cloth_Static::CAnalysis2D_Cloth_Static()
{
	face_color[0] = 1.0;
	face_color[1] = 1.0;
	face_color[2] = 1.0;
	tex_scale = 1;
	////
	total_energy_ref = -1;
	aSolSens.resize(10);
	total_energy_ = 0;
	is_detail_ = true;
	pDF_detail = 0;
	pDF = 0;
	cur_time_ = 0;
	pCT = 0;
	this->is_draw_pattern_boundary = true;
	this->is_lighting_ = false;
	this->is_show_edge_ = 0;
	this->imode_sensitivity_guess_ = 4;
	this->imode_ = CLOTH_INITIAL_LOCATION;
}

void CAnalysis2D_Cloth_Static::SetIsShowEdge(bool is_show)
{
	if (IsShowEdge() == is_show) return;
	is_show_edge_ = is_show;
	InitDrawer();
}

void CAnalysis2D_Cloth_Static::SetIsLighting(bool is_lighting)
{
	if (IsLighting() == is_lighting) return;
	this->is_lighting_ = is_lighting;
	InitDrawer();
}


Com::CBoundingBox3D CAnalysis2D_Cloth_Static::GetBoundingBox(double rot[]) const
{
	Com::CBoundingBox3D bb(-1, 1, -1, 1, -1, 1);
	if (!obj_mesh.IsEmpty())
	{
		double c[3], w[3]; obj_mesh.GetCenterWidth(c[0], c[1], c[2], w[0], w[1], w[2]);
		bb.x_min = c[0] - w[0] * 0.5;
		bb.x_max = c[0] + w[0] * 0.5;
		bb.y_min = c[1] - w[1] * 0.5;
		bb.y_max = c[1] + w[1] * 0.5;
		bb.z_min = c[2] - w[2] * 0.5;
		bb.z_max = c[2] + w[2] * 0.5;
	}
	return bb;
}


void CAnalysis2D_Cloth_Static::Serialize(Com::CSerializer& arch)
{
	if (arch.IsLoading())
	{
		const unsigned int buff_size = 256;
		char class_name[buff_size];
		arch.ReadDepthClassName(class_name, buff_size);
		assert(strncmp(class_name, "CAnalysis2D_Cloth", 27) == 0);
	}
	else
	{
		arch.WriteDepthClassName("CAnalysis2D_Cloth");
	}
}

void CAnalysis2D_Cloth_Static::SetModelProblem_Cloth
(Cad::CCadObj2D_Move& cad_2d, Msh::CMesher2D& mesh_2d, unsigned int inum_problem_,
CSliderDeform& slider_deform,
std::vector< std::pair<unsigned int, unsigned int> >& aSymIdVPair)
{
	cad_2d.Clear();
	mesh_2d.Clear();
	stitch_ary_.Clear();
	aIdECad_Fix.clear();
	slider_deform.Clear();
	aSymIdVPair.clear();

	unsigned int id_l1 = 0, id_l2 = 0, id_l3 = 0, id_l4 = 0;
	std::vector< std::pair<unsigned int, unsigned int> > aIdECad_Stitch;
	if (inum_problem_ == 6)
	{
		Cad::CCadObj2D::CResAddPolygon res1;
		{	// define shape
			std::vector<Com::CVector2D> vec_ary;
			vec_ary.push_back(Com::CVector2D(0.8 - 0.25, +0.2));  // 0
			vec_ary.push_back(Com::CVector2D(0.8 + 0.25, +0.2));  // 1
			vec_ary.push_back(Com::CVector2D(0.8 + 0.25, +0.60));  // 2
			vec_ary.push_back(Com::CVector2D(0.8 + 0.20, +0.80));  // 3
			////
			vec_ary.push_back(Com::CVector2D(0.8 + 0.10, +0.85));  // 4
			vec_ary.push_back(Com::CVector2D(0.8 - 0.10, +0.85));  // 5
			////
			vec_ary.push_back(Com::CVector2D(0.8 - 0.20, +0.80));  // 6
			vec_ary.push_back(Com::CVector2D(0.8 - 0.25, +0.60));  // 7
			if (inum_problem_ == 6)
			{
				for (unsigned int ivec = 0; ivec < vec_ary.size(); ivec++) { vec_ary[ivec] = 1.6*vec_ary[ivec]; }
			}
			res1 = cad_2d.AddPolygon(vec_ary);
			cad_2d.SetCurve_Polyline(res1.aIdE[2]);
			cad_2d.PreCompDragPolyline(res1.aIdE[2], vec_ary[2] * 0.4 + vec_ary[3] * 0.6);
			cad_2d.DragPolyline(res1.aIdE[2], vec_ary[2] * 0.4 + vec_ary[3] * 0.6 + Com::CVector2D(-0.05, -0.03));
			cad_2d.SetCurve_Polyline(res1.aIdE[6]);
			cad_2d.PreCompDragPolyline(res1.aIdE[6], vec_ary[7] * 0.4 + vec_ary[6] * 0.6);
			cad_2d.DragPolyline(res1.aIdE[6], vec_ary[7] * 0.4 + vec_ary[6] * 0.6 + Com::CVector2D(+0.05, -0.03));
			aSymIdVPair.push_back(std::make_pair(res1.aIdV[1], res1.aIdV[0]));
			aSymIdVPair.push_back(std::make_pair(res1.aIdV[2], res1.aIdV[7]));
			aSymIdVPair.push_back(std::make_pair(res1.aIdV[3], res1.aIdV[6]));
			aSymIdVPair.push_back(std::make_pair(res1.aIdV[4], res1.aIdV[5]));

#if 1
			// ldp test: add an non-manifold edge, failed
			auto v_id0 = cad_2d.AddVertex(Cad::CAD_ELEM_TYPE::LOOP, res1.id_l_add,
				(vec_ary[0] * 0.4 + vec_ary[1] * 0.6 + vec_ary[4] * 0.4 + vec_ary[5] * 0.6) * 0.5);
			auto v_id1 = cad_2d.AddVertex(Cad::CAD_ELEM_TYPE::EDGE, res1.aIdE[0],
				(vec_ary[0] * 0.4 + vec_ary[1] * 0.6 + vec_ary[0] * 0.4 + vec_ary[1] * 0.6) * 0.5);
			res1.aIdV.push_back(v_id0.id_v_add);
			res1.aIdV.push_back(v_id1.id_v_add);
			auto e_id_01 = cad_2d.ConnectVertex_Line(res1.aIdV[res1.aIdV.size() - 2], res1.aIdV[res1.aIdV.size() - 1]);
			res1.aIdE.push_back(e_id_01.id_e_add);
			assert(cad_2d.AssertValid() == 0);
			auto v_id2 = cad_2d.AddVertex(Cad::CAD_ELEM_TYPE::LOOP, res1.id_l_add,
				(vec_ary[0] * 0.6 + vec_ary[1] * 0.4 + vec_ary[4] * 0.6 + vec_ary[5] * 0.4) * 0.5);
			auto v_id3 = cad_2d.AddVertex(Cad::CAD_ELEM_TYPE::EDGE, res1.aIdE[0],
				(vec_ary[0] * 0.6 + vec_ary[1] * 0.4 + vec_ary[0] * 0.6 + vec_ary[1] * 0.4) * 0.5);
			res1.aIdV.push_back(v_id2.id_v_add);
			res1.aIdV.push_back(v_id3.id_v_add);
			auto e_id_23 = cad_2d.ConnectVertex_Line(res1.aIdV[res1.aIdV.size() - 2], res1.aIdV[res1.aIdV.size() - 1]);
			res1.aIdE.push_back(e_id_23.id_e_add);
			aIdECad_Stitch.push_back(std::make_pair(res1.aIdE[res1.aIdE.size() - 2], res1.aIdE[res1.aIdE.size() - 1]));
#endif
		}
		Cad::CCadObj2D::CResAddPolygon res2;
		{	// define shape
			std::vector<Com::CVector2D> vec_ary;
			vec_ary.push_back(Com::CVector2D(-0.25, +0.2)); // 0
			vec_ary.push_back(Com::CVector2D(+0.25, +0.2)); // 1
			vec_ary.push_back(Com::CVector2D(+0.25, +0.60)); // 2
			vec_ary.push_back(Com::CVector2D(+0.20, +0.80)); // 3
			////
			vec_ary.push_back(Com::CVector2D(+0.10, +0.85)); // 4           
			vec_ary.push_back(Com::CVector2D(+0.00, +0.5)); // 5   
			vec_ary.push_back(Com::CVector2D(-0.10, +0.85)); // 6
			////
			vec_ary.push_back(Com::CVector2D(-0.20, +0.80)); // 7
			vec_ary.push_back(Com::CVector2D(-0.25, +0.60)); // 8 
			if (inum_problem_ == 6)
			{
				for (unsigned int ivec = 0; ivec < vec_ary.size(); ivec++) { vec_ary[ivec] = 1.6*vec_ary[ivec]; }
			}
			res2 = cad_2d.AddPolygon(vec_ary);
			cad_2d.SetCurve_Polyline(res2.aIdE[2]);
			cad_2d.PreCompDragPolyline(res2.aIdE[2], vec_ary[2] * 0.4 + vec_ary[3] * 0.6);
			cad_2d.DragPolyline(res2.aIdE[2], vec_ary[2] * 0.4 + vec_ary[3] * 0.6 + Com::CVector2D(-0.05, -0.03));
			cad_2d.SetCurve_Polyline(res2.aIdE[7]);
			cad_2d.PreCompDragPolyline(res2.aIdE[7], vec_ary[8] * 0.4 + vec_ary[7] * 0.6);
			cad_2d.DragPolyline(res2.aIdE[7], vec_ary[8] * 0.4 + vec_ary[7] * 0.6 + Com::CVector2D(+0.05, -0.03));
			aSymIdVPair.push_back(std::make_pair(res2.aIdV[1], res2.aIdV[0]));
			aSymIdVPair.push_back(std::make_pair(res2.aIdV[2], res2.aIdV[8]));
			aSymIdVPair.push_back(std::make_pair(res2.aIdV[3], res2.aIdV[7]));
			aSymIdVPair.push_back(std::make_pair(res2.aIdV[4], res2.aIdV[6]));
		}
		id_l1 = res1.id_l_add;
		id_l2 = res2.id_l_add;
		mesh_2d.AddIdLCad_CutMesh(res1.id_l_add);
		mesh_2d.AddIdLCad_CutMesh(res2.id_l_add);
		aIdECad_Stitch.push_back(std::make_pair(res1.aIdE[1], res2.aIdE[8]));
		aIdECad_Stitch.push_back(std::make_pair(res1.aIdE[7], res2.aIdE[1]));
		aIdECad_Stitch.push_back(std::make_pair(res1.aIdE[3], res2.aIdE[6]));
		aIdECad_Stitch.push_back(std::make_pair(res1.aIdE[5], res2.aIdE[3]));
	}
	if (inum_problem_ == 9)
	{
		Cad::CCadObj2D::CResAddPolygon res1;
		{	// define shape
			std::vector<Com::CVector2D> vec_ary;
			vec_ary.push_back(Com::CVector2D(0.8 - 0.25, +0.2));  // 0
			vec_ary.push_back(Com::CVector2D(0.8 + 0.25, +0.2));  // 1
			vec_ary.push_back(Com::CVector2D(0.8 + 0.25, +0.50));  // 2
			vec_ary.push_back(Com::CVector2D(0.8 + 0.20, +0.75));  // 3
			////
			vec_ary.push_back(Com::CVector2D(0.8 + 0.10, +0.8));  // 4
			vec_ary.push_back(Com::CVector2D(0.8 - 0.10, +0.8));  // 5
			////
			vec_ary.push_back(Com::CVector2D(0.8 - 0.20, +0.75));  // 6
			vec_ary.push_back(Com::CVector2D(0.8 - 0.25, +0.50));  // 7
			if (inum_problem_ == 9)
			{
				for (unsigned int ivec = 0; ivec < vec_ary.size(); ivec++) { vec_ary[ivec] = 1.6*vec_ary[ivec]; }
			}
			res1 = cad_2d.AddPolygon(vec_ary);
			cad_2d.SetCurve_Polyline(res1.aIdE[2]);
			cad_2d.PreCompDragPolyline(res1.aIdE[2], vec_ary[2] * 0.4 + vec_ary[3] * 0.6);
			cad_2d.DragPolyline(res1.aIdE[2], vec_ary[2] * 0.4 + vec_ary[3] * 0.6 + Com::CVector2D(-0.05, -0.03));
			cad_2d.SetCurve_Polyline(res1.aIdE[6]);
			cad_2d.PreCompDragPolyline(res1.aIdE[6], vec_ary[7] * 0.4 + vec_ary[6] * 0.6);
			cad_2d.DragPolyline(res1.aIdE[6], vec_ary[7] * 0.4 + vec_ary[6] * 0.6 + Com::CVector2D(+0.05, -0.03));
			aSymIdVPair.push_back(std::make_pair(res1.aIdV[1], res1.aIdV[0]));
			aSymIdVPair.push_back(std::make_pair(res1.aIdV[2], res1.aIdV[7]));
			aSymIdVPair.push_back(std::make_pair(res1.aIdV[3], res1.aIdV[6]));
			aSymIdVPair.push_back(std::make_pair(res1.aIdV[4], res1.aIdV[5]));
		}
		Cad::CCadObj2D::CResAddPolygon res2;
		{	// define shape
			std::vector<Com::CVector2D> vec_ary;
			vec_ary.push_back(Com::CVector2D(-0.25, +0.2)); // 0
			vec_ary.push_back(Com::CVector2D(+0.25, +0.2)); // 1
			vec_ary.push_back(Com::CVector2D(+0.25, +0.50)); // 2
			vec_ary.push_back(Com::CVector2D(+0.20, +0.75)); // 3
			////
			vec_ary.push_back(Com::CVector2D(+0.10, +0.8)); // 4           
			vec_ary.push_back(Com::CVector2D(+0.00, +0.6)); // 5   
			vec_ary.push_back(Com::CVector2D(-0.10, +0.8)); // 6
			////
			vec_ary.push_back(Com::CVector2D(-0.20, +0.75)); // 7
			vec_ary.push_back(Com::CVector2D(-0.25, +0.50)); // 8 
			if (inum_problem_ == 9)
			{
				for (unsigned int ivec = 0; ivec < vec_ary.size(); ivec++) { vec_ary[ivec] = 1.6*vec_ary[ivec]; }
			}
			res2 = cad_2d.AddPolygon(vec_ary);
			cad_2d.SetCurve_Polyline(res2.aIdE[2]);
			cad_2d.PreCompDragPolyline(res2.aIdE[2], vec_ary[2] * 0.4 + vec_ary[3] * 0.6);
			cad_2d.DragPolyline(res2.aIdE[2], vec_ary[2] * 0.4 + vec_ary[3] * 0.6 + Com::CVector2D(-0.05, -0.03));
			cad_2d.SetCurve_Polyline(res2.aIdE[7]);
			cad_2d.PreCompDragPolyline(res2.aIdE[7], vec_ary[8] * 0.4 + vec_ary[7] * 0.6);
			cad_2d.DragPolyline(res2.aIdE[7], vec_ary[8] * 0.4 + vec_ary[7] * 0.6 + Com::CVector2D(+0.05, -0.03));
			aSymIdVPair.push_back(std::make_pair(res2.aIdV[1], res2.aIdV[0]));
			aSymIdVPair.push_back(std::make_pair(res2.aIdV[2], res2.aIdV[8]));
			aSymIdVPair.push_back(std::make_pair(res2.aIdV[3], res2.aIdV[7]));
			aSymIdVPair.push_back(std::make_pair(res2.aIdV[4], res2.aIdV[6]));
		}

		Cad::CCadObj2D::CResAddPolygon res3;
		{	// define shape
			std::vector<Com::CVector2D> vec_ary;
			vec_ary.push_back(Com::CVector2D(+0.05, -0.7));
			vec_ary.push_back(Com::CVector2D(+0.10, -0.7));
			vec_ary.push_back(Com::CVector2D(+0.10, -0.3));
			vec_ary.push_back(Com::CVector2D(+0.05, -0.3));
			vec_ary.push_back(Com::CVector2D(-0.10, -0.5));
			for (unsigned int ivec = 0; ivec < vec_ary.size(); ivec++) { vec_ary[ivec] = 1.6*vec_ary[ivec]; }
			res3 = cad_2d.AddPolygon(vec_ary);
			cad_2d.SetCurve_Polyline(res3.aIdE[3]);
			cad_2d.PreCompDragPolyline(res3.aIdE[3], vec_ary[3] * 0.4 + vec_ary[4] * 0.6);
			cad_2d.DragPolyline(res3.aIdE[3], vec_ary[3] * 0.4 + vec_ary[4] * 0.6 + Com::CVector2D(-0.03, -0.0));
			cad_2d.SetCurve_Polyline(res3.aIdE[4]);
			cad_2d.PreCompDragPolyline(res3.aIdE[4], vec_ary[4] * 0.6 + vec_ary[0] * 0.4);
			cad_2d.DragPolyline(res3.aIdE[4], vec_ary[4] * 0.6 + vec_ary[0] * 0.4 + Com::CVector2D(-0.03, -0.0));
		}
		Cad::CCadObj2D::CResAddPolygon res4;
		{	// define shape
			std::vector<Com::CVector2D> vec_ary;
			vec_ary.push_back(Com::CVector2D(+0.60, -0.7));
			vec_ary.push_back(Com::CVector2D(+0.65, -0.7));
			vec_ary.push_back(Com::CVector2D(+0.80, -0.5));
			vec_ary.push_back(Com::CVector2D(+0.65, -0.3));
			vec_ary.push_back(Com::CVector2D(+0.60, -0.3));
			for (unsigned int ivec = 0; ivec < vec_ary.size(); ivec++) { vec_ary[ivec] = 1.6*vec_ary[ivec]; }
			res4 = cad_2d.AddPolygon(vec_ary);
			cad_2d.SetCurve_Polyline(res4.aIdE[1]);
			cad_2d.PreCompDragPolyline(res4.aIdE[1], vec_ary[1] * 0.4 + vec_ary[2] * 0.6);
			cad_2d.DragPolyline(res4.aIdE[1], vec_ary[1] * 0.4 + vec_ary[2] * 0.6 + Com::CVector2D(+0.03, -0.0));
			cad_2d.SetCurve_Polyline(res4.aIdE[2]);
			cad_2d.PreCompDragPolyline(res4.aIdE[2], vec_ary[2] * 0.6 + vec_ary[3] * 0.4);
			cad_2d.DragPolyline(res4.aIdE[2], vec_ary[2] * 0.6 + vec_ary[3] * 0.4 + Com::CVector2D(+0.03, -0.0));
		}
		aSymIdVPair.push_back(std::make_pair(res3.aIdV[0], res4.aIdV[1]));
		aSymIdVPair.push_back(std::make_pair(res3.aIdV[1], res4.aIdV[0]));
		aSymIdVPair.push_back(std::make_pair(res3.aIdV[2], res4.aIdV[4]));
		aSymIdVPair.push_back(std::make_pair(res3.aIdV[3], res4.aIdV[3]));
		aSymIdVPair.push_back(std::make_pair(res3.aIdV[4], res4.aIdV[2]));

		id_l1 = res1.id_l_add;
		id_l2 = res2.id_l_add;
		id_l3 = res3.id_l_add;
		id_l4 = res4.id_l_add;
		mesh_2d.AddIdLCad_CutMesh(res1.id_l_add);
		mesh_2d.AddIdLCad_CutMesh(res2.id_l_add);
		mesh_2d.AddIdLCad_CutMesh(res3.id_l_add);
		mesh_2d.AddIdLCad_CutMesh(res4.id_l_add);
		aIdECad_Stitch.push_back(std::make_pair(res1.aIdE[1], res2.aIdE[8]));
		aIdECad_Stitch.push_back(std::make_pair(res1.aIdE[7], res2.aIdE[1]));
		aIdECad_Stitch.push_back(std::make_pair(res1.aIdE[3], res2.aIdE[6]));
		aIdECad_Stitch.push_back(std::make_pair(res1.aIdE[5], res2.aIdE[3]));
		////
		aIdECad_Stitch.push_back(std::make_pair(res2.aIdE[2], res3.aIdE[4]));
		aIdECad_Stitch.push_back(std::make_pair(res1.aIdE[6], res3.aIdE[3]));
		aIdECad_Stitch.push_back(std::make_pair(res3.aIdE[0], res3.aIdE[2]));
		////
		aIdECad_Stitch.push_back(std::make_pair(res2.aIdE[7], res4.aIdE[1]));
		aIdECad_Stitch.push_back(std::make_pair(res1.aIdE[2], res4.aIdE[2]));
		aIdECad_Stitch.push_back(std::make_pair(res4.aIdE[0], res4.aIdE[3]));
	}
	if (inum_problem_ == 11)
	{
		Cad::CCadObj2D::CResAddPolygon res1;
		{	// define shape
			std::vector<Com::CVector2D> vec_ary;
			{
				vec_ary.push_back(Com::CVector2D(0.8 - 0.25, +0.5));  // 0
				vec_ary.push_back(Com::CVector2D(0.8 + 0.25, +0.5));  // 1
				vec_ary.push_back(Com::CVector2D(0.8 + 0.25, +0.60));  // 2
				vec_ary.push_back(Com::CVector2D(0.8 + 0.20, +0.85));  // 3
				////
				vec_ary.push_back(Com::CVector2D(0.8 + 0.10, +0.9));  // 4
				vec_ary.push_back(Com::CVector2D(0.8 - 0.10, +0.9));  // 5
				////
				vec_ary.push_back(Com::CVector2D(0.8 - 0.20, +0.85));  // 6
				vec_ary.push_back(Com::CVector2D(0.8 - 0.25, +0.60));  // 7                
				for (unsigned int ivec = 0; ivec < vec_ary.size(); ivec++) { vec_ary[ivec] = 1.6*vec_ary[ivec]; }
			}
			res1 = cad_2d.AddPolygon(vec_ary);
			cad_2d.SetCurve_Polyline(res1.aIdE[2]);
			cad_2d.PreCompDragPolyline(res1.aIdE[2], vec_ary[2] * 0.4 + vec_ary[3] * 0.6);
			cad_2d.DragPolyline(res1.aIdE[2], vec_ary[2] * 0.4 + vec_ary[3] * 0.6 + Com::CVector2D(-0.05, -0.03));
			cad_2d.SetCurve_Polyline(res1.aIdE[6]);
			cad_2d.PreCompDragPolyline(res1.aIdE[6], vec_ary[7] * 0.4 + vec_ary[6] * 0.6);
			cad_2d.DragPolyline(res1.aIdE[6], vec_ary[7] * 0.4 + vec_ary[6] * 0.6 + Com::CVector2D(+0.05, -0.03));
			aSymIdVPair.push_back(std::make_pair(res1.aIdV[1], res1.aIdV[0]));
			aSymIdVPair.push_back(std::make_pair(res1.aIdV[2], res1.aIdV[7]));
			aSymIdVPair.push_back(std::make_pair(res1.aIdV[3], res1.aIdV[6]));
			aSymIdVPair.push_back(std::make_pair(res1.aIdV[4], res1.aIdV[5]));
		}
		Cad::CCadObj2D::CResAddPolygon res2;
		{	// define shape
			std::vector<Com::CVector2D> vec_ary;
			{
				vec_ary.push_back(Com::CVector2D(-0.25, +0.5)); // 0
				vec_ary.push_back(Com::CVector2D(+0.25, +0.5)); // 1
				vec_ary.push_back(Com::CVector2D(+0.25, +0.60)); // 2
				vec_ary.push_back(Com::CVector2D(+0.20, +0.85)); // 3
				////
				vec_ary.push_back(Com::CVector2D(+0.10, +0.9)); // 4           
				vec_ary.push_back(Com::CVector2D(+0.00, +0.6)); // 5   
				vec_ary.push_back(Com::CVector2D(-0.10, +0.9)); // 6
				////
				vec_ary.push_back(Com::CVector2D(-0.20, +0.85)); // 7
				vec_ary.push_back(Com::CVector2D(-0.25, +0.60)); // 8         
				for (unsigned int ivec = 0; ivec < vec_ary.size(); ivec++) { vec_ary[ivec] = 1.6*vec_ary[ivec]; }
			}
			res2 = cad_2d.AddPolygon(vec_ary);
			cad_2d.SetCurve_Polyline(res2.aIdE[2]);
			cad_2d.PreCompDragPolyline(res2.aIdE[2], vec_ary[2] * 0.4 + vec_ary[3] * 0.6);
			cad_2d.DragPolyline(res2.aIdE[2], vec_ary[2] * 0.4 + vec_ary[3] * 0.6 + Com::CVector2D(-0.05, -0.03));
			cad_2d.SetCurve_Polyline(res2.aIdE[7]);
			cad_2d.PreCompDragPolyline(res2.aIdE[7], vec_ary[8] * 0.4 + vec_ary[7] * 0.6);
			cad_2d.DragPolyline(res2.aIdE[7], vec_ary[8] * 0.4 + vec_ary[7] * 0.6 + Com::CVector2D(+0.05, -0.03));
			aSymIdVPair.push_back(std::make_pair(res2.aIdV[1], res2.aIdV[0]));
			aSymIdVPair.push_back(std::make_pair(res2.aIdV[2], res2.aIdV[8]));
			aSymIdVPair.push_back(std::make_pair(res2.aIdV[3], res2.aIdV[7]));
			aSymIdVPair.push_back(std::make_pair(res2.aIdV[4], res2.aIdV[6]));
		}
		Cad::CCadObj2D::CResAddPolygon res3;
		{	// define shape
			std::vector<Com::CVector2D> vec_ary;
			{
				vec_ary.push_back(Com::CVector2D(0.8 - 0.30, -0.8 + 0.6));
				vec_ary.push_back(Com::CVector2D(0.8 + 0.30, -0.8 + 0.6));
				vec_ary.push_back(Com::CVector2D(0.8 + 0.18, -0.8 + 0.8));
				vec_ary.push_back(Com::CVector2D(0.8 - 0.18, -0.8 + 0.8));
				for (unsigned int ivec = 0; ivec < vec_ary.size(); ivec++) { vec_ary[ivec] = 1.6*vec_ary[ivec]; }
			}
			res3 = cad_2d.AddPolygon(vec_ary, 0);
			aSymIdVPair.push_back(std::make_pair(res3.aIdV[0], res3.aIdV[1]));
			aSymIdVPair.push_back(std::make_pair(res3.aIdV[2], res3.aIdV[3]));
		}
		Cad::CCadObj2D::CResAddPolygon res4;
		{	// define shape
			std::vector<Com::CVector2D> vec_ary;
			{
				vec_ary.push_back(Com::CVector2D(-0.30, -0.8 + 0.6));
				vec_ary.push_back(Com::CVector2D(+0.30, -0.8 + 0.6));
				vec_ary.push_back(Com::CVector2D(+0.18, -0.8 + 0.8));
				vec_ary.push_back(Com::CVector2D(-0.18, -0.8 + 0.8));
				for (unsigned int ivec = 0; ivec < vec_ary.size(); ivec++) { vec_ary[ivec] = 1.6*vec_ary[ivec]; }
			}
			res4 = cad_2d.AddPolygon(vec_ary, 0);
			aSymIdVPair.push_back(std::make_pair(res4.aIdV[0], res4.aIdV[1]));
			aSymIdVPair.push_back(std::make_pair(res4.aIdV[2], res4.aIdV[3]));
		}
		id_l1 = res1.id_l_add;
		id_l2 = res2.id_l_add;
		id_l3 = res3.id_l_add;
		id_l4 = res4.id_l_add;
		std::cout << "ID L4 " << id_l4 << std::endl;
		mesh_2d.AddIdLCad_CutMesh(res1.id_l_add);
		mesh_2d.AddIdLCad_CutMesh(res2.id_l_add);
		mesh_2d.AddIdLCad_CutMesh(res3.id_l_add);
		mesh_2d.AddIdLCad_CutMesh(res4.id_l_add);
		aIdECad_Stitch.push_back(std::make_pair(res1.aIdE[1], res2.aIdE[8]));
		aIdECad_Stitch.push_back(std::make_pair(res1.aIdE[7], res2.aIdE[1]));
		aIdECad_Stitch.push_back(std::make_pair(res1.aIdE[3], res2.aIdE[6]));
		aIdECad_Stitch.push_back(std::make_pair(res1.aIdE[5], res2.aIdE[3]));
		////
		aIdECad_Stitch.push_back(std::make_pair(res3.aIdE[1], res4.aIdE[3]));
		aIdECad_Stitch.push_back(std::make_pair(res3.aIdE[3], res4.aIdE[1]));
		////
		aIdECad_Stitch.push_back(std::make_pair(res3.aIdE[2], res1.aIdE[0]));
		aIdECad_Stitch.push_back(std::make_pair(res4.aIdE[2], res2.aIdE[0]));
	}
	else if (inum_problem_ == 10)
	{
		{	// define shape
			std::vector<Com::CVector2D> vec_ary;
			{
				vec_ary.push_back(Com::CVector2D(-0.35, -0.35));
				vec_ary.push_back(Com::CVector2D(+0.35, -0.35));
				vec_ary.push_back(Com::CVector2D(+0.35, +0.35));
				vec_ary.push_back(Com::CVector2D(-0.35, +0.35));
			}
			id_l1 = cad_2d.AddPolygon(vec_ary).id_l_add;
		}
		mesh_2d.AddIdLCad_CutMesh(id_l1);
	}

	//  mesh_2d.SetMeshingMode_ElemSize(3500);  
	mesh_2d.SetMeshingMode_ElemSize(3000);
	mesh_2d.Meshing(cad_2d);
	std::cout << "Node size : " << mesh_2d.GetVectorAry().size() << std::endl;

	for (unsigned int ist = 0; ist < aIdECad_Stitch.size(); ist++)
	{
		const unsigned int id_e1 = aIdECad_Stitch[ist].first;
		const unsigned int id_e2 = aIdECad_Stitch[ist].second;
		stitch_ary_.AddStitch(cad_2d, mesh_2d, id_e1, id_e2);
	}
	std::cout << "stitch size : " << stitch_ary_.aStitch.size() << std::endl;

	/////
	clothHandler_.Clear();
	if (pCT != 0) { delete pCT; pCT = 0; }
	else if (
		inum_problem_ == 6
		|| inum_problem_ == 9
		|| inum_problem_ == 11)
	{
		obj_mesh.SetIsNormal(true);
		//    obj_mesh.Load_Ply("../model/kid_15k.ply");    
		CSurfaceMeshReader cnt_mesh;
		{
			//obj_mesh.Load_Ply("models/arm_16k.ply");
			//cnt_mesh.Load_Ply("models/arm_cnt.ply");
			obj_mesh.Load_Ply("models/wm2_15k.ply");
			cnt_mesh.Load_Ply("models/wm2_cnt.ply");
		}
		//    cnt_mesh.Load_Ply("../model/kid_cnt.ply");
		double c[3], w[3]; obj_mesh.GetCenterWidth(c[0], c[1], c[2], w[0], w[1], w[2]);
		//    double scale = 1.1/w[1];  
		{
			double scale = 1.5 / w[1];
			obj_mesh.Translate(-c[0], -c[1], -c[2]);  obj_mesh.Scale(scale);  obj_mesh.Rot_Bryant(90, 0, 180);
			cnt_mesh.Translate(-c[0], -c[1], -c[2]);  cnt_mesh.Scale(scale);  cnt_mesh.Rot_Bryant(90, 0, 180);
			double c[3], w[3]; obj_mesh.GetCenterWidth(c[0], c[1], c[2], w[0], w[1], w[2]);
			std::cout << "Scaled Armadillo " << " " << w[0] << " " << w[1] << " " << w[2] << std::endl;
		}
		////
		std::vector<unsigned int> aTri;
		std::vector<double> aXYZ;
		cnt_mesh.GetMesh(aTri, aXYZ);
		clothHandler_.SetObjectMesh(aTri, aXYZ);
		////
		CContactTarget3D_Mesh* pCT1 = new CContactTarget3D_Mesh;
		//		pCT1->Load_Off("venus.off");			
		//		pCT1->Load_Ply("girl3.ply");			    
		//		pCT1->Load_Ply("../model/kid_cnt.ply");
		pCT1->SetMesh(aTri, aXYZ);
		pCT1->BuildBoxel();
		pCT1->SetHole(false);
		pCT = new CContactTarget3D_AdaptiveDistanceField3D();
		double bb[6] = { -1, 1, -1, 1, -1, 1 };
		((CContactTarget3D_AdaptiveDistanceField3D*)pCT)->SetUp(*pCT1, bb);
		((CContactTarget3D_AdaptiveDistanceField3D*)pCT)->BuildMarchingCubeEdge();
	}
	if (inum_problem_ == 10)
	{
		double org[3] = { 0.0, 0.0, -0.10 };
		pCT = new CContactTarget3D_Sphere(0.11, org, true);
		std::vector<unsigned int> aTri;
		std::vector<double> aXYZ;
		pCT->GetMesh(aTri, aXYZ, 1);
		clothHandler_.SetObjectMesh(aTri, aXYZ);
	}

	std::cout << "inum_problem : " << inum_problem_ << std::endl;


	if (inum_problem_ == 6)
	{
		{
			clothHandler_.AddClothPiece(id_l1, +0.8*1.6, +0.5*1.6);
			clothHandler_.Transform_Cloth_Pan(id_l1, +0.0*1.6, +0.20*1.6, +0.15*1.6);
			clothHandler_.Transform_Cloth_RotBryantAngle(id_l1, 90, 0, 180);
			////
			clothHandler_.AddClothPiece(id_l2, +0.0*1.6, +0.5*1.6);
			clothHandler_.Transform_Cloth_Pan(id_l2, +0.0*1.6, -0.10*1.6, -0.0*1.6);
			clothHandler_.Transform_Cloth_RotBryantAngle(id_l2, 90, 0, 0);
		}
		////////
		const unsigned int is1 = slider_deform.AddSlider("length", 0, 0, 1);
		slider_deform.AddSliderParamToLoop(id_l1, is1, 1, 3);
		slider_deform.AddSliderParamToLoop(id_l2, is1, 1, 3);
		////
		const unsigned int is2 = slider_deform.AddSlider("waist", 0, -0.5, 0.5);
		slider_deform.AddSliderParamToLoop(id_l1, is2, 0, 4);
		slider_deform.AddSliderParamToLoop(id_l2, is2, 0, 4);
		/////
		slider_deform.SetLoopCenter(id_l1, 0.8, 0.5);
		slider_deform.SetLoopCenter(id_l2, 0.0, 0.5);
	}
	else if (inum_problem_ == 9)
	{
		{
			clothHandler_.AddClothPiece(id_l1, +0.8*1.6, +0.5*1.6);
			clothHandler_.Transform_Cloth_Pan(id_l1, +0.0, +0.15*1.6, +0.2);
			clothHandler_.Transform_Cloth_RotBryantAngle(id_l1, 90, 0, 180);
			////
			clothHandler_.AddClothPiece(id_l2, +0.0, +0.5*1.6);
			clothHandler_.Transform_Cloth_Pan(id_l2, +0.0, -0.15*1.6, +0.1);
			clothHandler_.Transform_Cloth_RotBryantAngle(id_l2, 90, 0, 0);
			////
			clothHandler_.AddClothPiece(id_l3, +0.05*1.6, -0.5*1.6);
			clothHandler_.Transform_Cloth_Pan(id_l3, +0.3*1.6, +0.0, +0.3*1.6);
			clothHandler_.Transform_Cloth_RotBryantAngle(id_l3, 0, 0, -30);
			//    clothHandler_.Transform_Cloth_RotBryantAngle(id_l3, 0, 0, 0);           
			clothHandler_.SetRadius(id_l3, 0.09*1.6, false);
			////
			clothHandler_.AddClothPiece(id_l4, +0.65*1.6, -0.5*1.6);
			clothHandler_.Transform_Cloth_Pan(id_l4, -0.3*1.6, +0.0, +0.3*1.6);
			clothHandler_.Transform_Cloth_RotBryantAngle(id_l4, 0, 0, +30);
			clothHandler_.SetRadius(id_l4, 0.09*1.6, false);
		}
		////////
		const unsigned int is1 = slider_deform.AddSlider("length", 0, 0, 1);
		slider_deform.AddSliderParamToLoop(id_l1, is1, 1, 3);
		slider_deform.AddSliderParamToLoop(id_l2, is1, 1, 3);
		////
		const unsigned int is2 = slider_deform.AddSlider("waist", 0, -0.5, 0.5);
		slider_deform.AddSliderParamToLoop(id_l1, is2, 0, 4);
		slider_deform.AddSliderParamToLoop(id_l2, is2, 0, 4);
		////
		const unsigned int is3 = slider_deform.AddSlider("sleeve length", 0, -0.5, 1);
		slider_deform.AddSliderParamToLoop(id_l3, is3, 0, 0);
		slider_deform.AddSliderParamToLoop(id_l4, is3, 0, 1);
		////
		slider_deform.SetLoopCenter(id_l1, 0.8, 0.5);
		slider_deform.SetLoopCenter(id_l2, 0.0, 0.5);
		slider_deform.SetLoopCenter(id_l3, +0.05, -0.5);
		slider_deform.SetLoopCenter(id_l4, +0.65, -0.5);
	}
	else if (inum_problem_ == 11)
	{
		{
			clothHandler_.AddClothPiece(id_l1, +0.8*1.6, +0.5*1.6);
			clothHandler_.Transform_Cloth_Pan(id_l1, +0.0, +0.20*1.6, +0.05);
			clothHandler_.Transform_Cloth_RotBryantAngle(id_l1, 90, 0, 180);
			////
			clothHandler_.AddClothPiece(id_l2, +0.0, +0.5*1.6);
			clothHandler_.Transform_Cloth_Pan(id_l2, +0.0, -0.10*1.6, +0.05);
			clothHandler_.Transform_Cloth_RotBryantAngle(id_l2, 90, 0, 0);
			/////
			clothHandler_.AddClothPiece(id_l3, +0.8*1.6, -0.8*1.6);
			clothHandler_.Transform_Cloth_Pan(id_l3, +0.0, +0.20*1.6, -1.3);
			clothHandler_.Transform_Cloth_RotBryantAngle(id_l3, 90, 0, 180);
			//    clothHandler_.SetRadius(id_l3, 0.09); 
			////
			clothHandler_.AddClothPiece(id_l4, +0.0, -0.8*1.6);
			clothHandler_.Transform_Cloth_Pan(id_l4, -0.0, -0.10*1.6, -1.3);
			clothHandler_.Transform_Cloth_RotBryantAngle(id_l4, 90, 0, 0);
		}
		//    clothHandler_.SetRadius(id_l4, 0.09);     
		////////
		const unsigned int is1 = slider_deform.AddSlider("upper length", 0, 0, 1);
		slider_deform.AddSliderParamToLoop(id_l1, is1, 1, 3);
		slider_deform.AddSliderParamToLoop(id_l2, is1, 1, 3);
		////
		const unsigned int is2 = slider_deform.AddSlider("upper waist", 0, -0.5, 0.5);
		slider_deform.AddSliderParamToLoop(id_l1, is2, 0, 3);
		slider_deform.AddSliderParamToLoop(id_l2, is2, 0, 3);
		/////
		const unsigned int is3 = slider_deform.AddSlider("down length", 0, 0, 1);
		slider_deform.AddSliderParamToLoop(id_l3, is3, 1, 3);
		slider_deform.AddSliderParamToLoop(id_l4, is3, 1, 3);
		////
		const unsigned int is4 = slider_deform.AddSlider("down waist", 0, -0.5, 0.5);
		slider_deform.AddSliderParamToLoop(id_l3, is4, 0, 2);
		slider_deform.AddSliderParamToLoop(id_l4, is4, 0, 2);
		////
		const unsigned int is5 = slider_deform.AddSlider("down fringe", 0, -0.5, 0.5);
		slider_deform.AddSliderParamToLoop(id_l3, is5, 0, 3);
		slider_deform.AddSliderParamToLoop(id_l4, is5, 0, 3);
		////
		slider_deform.SetLoopCenter(id_l1, 0.8, +0.5);
		slider_deform.SetLoopCenter(id_l2, 0.0, +0.5);
		slider_deform.SetLoopCenter(id_l3, 0.8, -0.1);
		slider_deform.SetLoopCenter(id_l4, 0.0, -0.1);
	}


	// time integ param
	cur_time_ = 0;
	dt_ = 0.005;
	gx_ = 0, gy_ = 0, gz_ = -2;
	//  gx_=0,gy_=0, gz_=-10;     

	// cloth parameters   
	//  cloth_param.stiff_bend = 1.0e-9;
	cloth_param.stiff_bend = 1.0e-10;
	//  cloth_param.stiff_bend = 0.0000;  
	cloth_param.stiff_myu = 0.2;
	cloth_param.stiff_lambda = 0.1;
	cloth_param.rho = 0.02;

	// contact parameters
	contact_param.myu_k = 0.3;
	contact_param.myu_s = 0.5;
	//  contact_param.myu_k = 0.0;  
	//  contact_param.myu_s = 0.0;  
	contact_param.stiff_n = 1;
	contact_param.stiff_f = 1;
	contact_param.offset = 0.01;
	contact_param.offset = 0.015;

	// stitch coeff
	stitch_ary_.SetStiff(1000);
	stitch_ary_.SetDampingCoeff(0.07);
	//  stitch_ary_.SetDampingCoeff(0.0);  
	this->imode_ = CLOTH_INITIAL_LOCATION;

	if (inum_problem_ == 10)
	{
		cloth_param.stiff_bend = 1.0 - 5;
	}
	if (inum_problem_ == 12 || inum_problem_ == 13)
	{
		this->is_detail_ = false;
		cloth_param.stiff_bend = 1.5e-3;
		cloth_param.stiff_myu = 20;
		dt_ = 0.0001;
	}


	this->InitFieldEqn_fromMsh(cad_2d, mesh_2d);
	this->ClearLinearSystemPreconditioner();
	this->InitDrawer();
	SaveTimeStamp(cad_2d, mesh_2d);
}

void CAnalysis2D_Cloth_Static::SetTextureScale_FaceFEM(double scale)
{
	tex_scale = scale;
	if (this->is_detail_)
	{
		if (pDF_detail == 0) return;
		pDF_detail->SetTexScale(scale, world);
	}
	else
	{
		if (pDF == 0) return;
		pDF->SetTexScale(scale, world);
	}
}

void CAnalysis2D_Cloth_Static::SetTextureCenter(double cx, double cy)
{
	//  std::cout << cx << " " << cy << std::endl;
	if (this->is_detail_)
	{
		if (pDF_detail == 0) return;
		pDF_detail->SetTexCenter(cx, cy);
	}
	else
	{
		if (pDF == 0) return;
		pDF->SetTexCenter(cx, cy);
	}
}

void CAnalysis2D_Cloth_Static::InitDrawer()
{
	// std::cout << "Init Drawer" << tex_scale << std::endl;
	double cnt_x = 0, cnt_y = 0;
	if (is_detail_) { if (pDF_detail != 0) { pDF_detail->GetTexCenter(cnt_x, cnt_y); } }
	else { if (pDF != 0) { pDF->GetTexCenter(cnt_x, cnt_y); } }
	////
	m_aDrawerField.Clear();
	m_aDrawerField_detail.Clear();
	this->pDF_detail = 0;
	this->pDF = 0;
	if (is_detail_)
	{
		{
			if (is_show_edge_)
			{
				View::CDrawerEdge *pDE = new View::CDrawerEdge(id_field_disp, false, world);
				pDE->SetLineWidth(2);
				m_aDrawerField.PushBack(pDE);
			}
		}
		{ // detail
		pDF_detail = new View::CDrawerFace(id_field_disp_detail, false, world);
		pDF_detail->EnableNormal(true);
		pDF_detail->EnableUVMap(true, world);
		pDF_detail->SetTexCenter(cnt_x, cnt_y);
		pDF_detail->SetTexScale(tex_scale, world);
		pDF_detail->SetColor(face_color[0], face_color[1], face_color[2]);
		m_aDrawerField_detail.PushBack(pDF_detail);
		if (is_show_edge_)
		{
			View::CDrawerEdge *pDE = new View::CDrawerEdge(id_field_disp_detail, false, world);
			pDE->SetLineWidth(1);
			m_aDrawerField_detail.PushBack(pDE);
		}
	}
	}
	else
	{
		{
			pDF = new View::CDrawerFace(id_field_disp, false, world);
			pDF->EnableNormal(true);
			pDF->EnableUVMap(true, world);
			pDF->SetTexCenter(cnt_x, cnt_y);
			pDF->SetTexScale(tex_scale, world);
			pDF->SetColor(face_color[0], face_color[1], face_color[2]);
			//      pDF->SetColor(0.8,0.8,0.8);      
			m_aDrawerField.PushBack(pDF);
			if (is_show_edge_)
			{
				View::CDrawerEdge *pDE = new View::CDrawerEdge(id_field_disp, false, world);
				pDE->SetLineWidth(2);
				m_aDrawerField.PushBack(pDE);
			}
		}
	}
}


void CAnalysis2D_Cloth_Static::PerformStaticSolver()
{
	//  std::cout << "perform static solver" << std::endl;
	if (imode_ != SIMULATION_STATIC)
	{
		this->ClearLinearSystemPreconditioner();
	}
	imode_ = SIMULATION_STATIC;
	CField& field = world.GetField(id_field_disp);
	CNodeAry::CNodeSeg& ns_v = field.GetNodeSeg(CORNER, true, world, VELOCITY);
	ns_v.SetZero();
	CopyValueVelo(id_field_disp_buffer, id_field_disp, world);
}

void CAnalysis2D_Cloth_Static::SetClothPiecePlacingMode()
{
	imode_ = CLOTH_INITIAL_LOCATION;
	clothHandler_.BuildClothMeshTopology(id_field_base, id_field_disp, world);
	clothHandler_.SetClothLocation(id_field_disp, world);
	if (is_detail_)
	{
		InitFineDeformInterp(aInterp_detail);
		MoveFineDeformedCoord(id_field_disp_detail, id_field_base_detail,
			id_field_disp, id_field_base,
			world,
			aInterp_detail);
		m_aDrawerField_detail.Update(world);
	}
	m_aDrawerField.Update(world);
}

void CAnalysis2D_Cloth_Static::Draw() const
{
	// Draw Hilighted Elem
	if (this->is_draw_pattern_boundary)
	{
		bool is_lighting = ::glIsEnabled(GL_LIGHTING);
		::glDisable(GL_LIGHTING);
		bool is_texture = ::glIsEnabled(GL_TEXTURE_2D);
		::glDisable(GL_TEXTURE_2D);
		::glEnable(GL_LINE_SMOOTH);
		::glEnable(GL_BLEND);
		if (!this->is_detail_)
		{
			if (!world.IsIdField(id_field_base)) return;
			const Fem::Field::CField& field_base = world.GetField(id_field_base);
			const std::vector<unsigned int>& aIdEA = field_base.GetAryIdEA();
			const Fem::Field::CIDConvEAMshCad& conv = world.GetIDConverter(id_field_base);
			const Fem::Field::CField& field_disp = world.GetField(id_field_disp);
			const Fem::Field::CNodeAry::CNodeSeg& ns_c = field_disp.GetNodeSeg(CORNER, false, world, VALUE);
			const Fem::Field::CNodeAry::CNodeSeg& ns_u = field_disp.GetNodeSeg(CORNER, true, world, VALUE);
			for (unsigned int iiea = 0; iiea < aIdEA.size(); iiea++)
			{
				unsigned int id_ea = aIdEA[iiea];
				//    unsigned int id_ea = conv.GetIdEA_fromCad(id_hilight_,itype_hilight_);
				if (!world.IsIdEA(id_ea)) { continue; }
				const Fem::Field::CElemAry::CElemSeg& es = field_disp.GetElemSeg(id_ea, CORNER, false, world);
				unsigned int id_part_cad;
				Cad::CAD_ELEM_TYPE itype_part_cad;
				conv.GetIdCad_fromIdEA(id_ea, id_part_cad, itype_part_cad);
				if (itype_part_cad == Cad::EDGE)
				{
					if (itype_hilight_ == Cad::EDGE && id_hilight_ == id_part_cad)
					{
						::glLineWidth(3);
						::glColor3d(1, 1, 0);
					}
					else
					{
						::glLineWidth(2);
						::glColor3d(0, 0, 0);
					}
					::glBegin(GL_LINES);
					for (unsigned int ie = 0; ie < es.Size(); ie++)
					{
						unsigned int no[2]; es.GetNodes(ie, no);
						double C[2][3]; ns_c.GetValue(no[0], C[0]); ns_c.GetValue(no[1], C[1]);
						double u[2][3]; ns_u.GetValue(no[0], u[0]); ns_u.GetValue(no[1], u[1]);
						double c[2][3] = { { C[0][0] + u[0][0], C[0][1] + u[0][1], C[0][2] + u[0][2] }, { C[1][0] + u[1][0], C[1][1] + u[1][1], C[1][2] + u[1][2] } };
						::glVertex3dv(c[0]);
						::glVertex3dv(c[1]);
					}
					::glEnd();
				}
				if (itype_part_cad == Cad::VERTEX)
				{
					::glPointSize(5);
					if (itype_hilight_ == Cad::VERTEX && id_hilight_ == id_part_cad) { ::glColor3d(1, 1, 0); }
					else { ::glColor3d(0, 0, 0); }
					::glBegin(GL_POINTS);
					for (unsigned int ie = 0; ie < es.Size(); ie++)
					{
						unsigned int no; es.GetNodes(ie, &no);
						double C[3]; ns_c.GetValue(no, C);
						double u[3]; ns_u.GetValue(no, u);
						double c[3] = { C[0] + u[0], C[1] + u[1], C[2] + u[2] };
						::glVertex3dv(c);
					}
					::glEnd();
				}
			}
		}
		else
		{
			::glDisable(GL_LIGHTING);
			::glDisable(GL_TEXTURE_2D);
			::glEnable(GL_LINE_SMOOTH);
			::glEnable(GL_BLEND);
			if (!world.IsIdField(id_field_base_detail)) return;
			const Fem::Field::CField& field_base = world.GetField(id_field_base_detail);
			const std::vector<unsigned int>& aIdEA = field_base.GetAryIdEA();
			const Fem::Field::CIDConvEAMshCad& conv = world.GetIDConverter(id_field_base_detail);
			const Fem::Field::CField& field_disp = world.GetField(id_field_disp_detail);
			const Fem::Field::CNodeAry::CNodeSeg& ns_c = field_disp.GetNodeSeg(CORNER, false, world, VALUE);
			const Fem::Field::CNodeAry::CNodeSeg& ns_u = field_disp.GetNodeSeg(CORNER, true, world, VALUE);
			for (unsigned int iiea = 0; iiea < aIdEA.size(); iiea++)
			{
				unsigned int id_ea = aIdEA[iiea];
				//    unsigned int id_ea = conv.GetIdEA_fromCad(id_hilight_,itype_hilight_);
				if (!world.IsIdEA(id_ea)) { continue; }
				const Fem::Field::CElemAry::CElemSeg& es = field_disp.GetElemSeg(id_ea, CORNER, false, world);
				unsigned int id_part_cad;
				Cad::CAD_ELEM_TYPE itype_part_cad;
				conv.GetIdCad_fromIdEA(id_ea, id_part_cad, itype_part_cad);
				if (itype_part_cad == Cad::EDGE)
				{
					if (itype_hilight_ == Cad::EDGE && id_hilight_ == id_part_cad)
					{
						::glLineWidth(3);
						::glColor3d(1, 1, 0);
					}
					else
					{
						::glLineWidth(2);
						::glColor3d(0, 0, 0);
					}
					::glBegin(GL_LINES);
					for (unsigned int ie = 0; ie < es.Size(); ie++)
					{
						unsigned int no[2]; es.GetNodes(ie, no);
						double C[2][3]; ns_c.GetValue(no[0], C[0]); ns_c.GetValue(no[1], C[1]);
						double u[2][3]; ns_u.GetValue(no[0], u[0]); ns_u.GetValue(no[1], u[1]);
						double c[2][3] = { { C[0][0] + u[0][0], C[0][1] + u[0][1], C[0][2] + u[0][2] }, { C[1][0] + u[1][0], C[1][1] + u[1][1], C[1][2] + u[1][2] } };
						::glVertex3dv(c[0]);
						::glVertex3dv(c[1]);
					}
					::glEnd();
				}
				if (itype_part_cad == Cad::VERTEX)
				{
					::glPointSize(5);
					if (itype_hilight_ == Cad::VERTEX && id_hilight_ == id_part_cad) { ::glColor3d(1, 1, 0); }
					else { ::glColor3d(0, 0, 0); }
					::glBegin(GL_POINTS);
					for (unsigned int ie = 0; ie < es.Size(); ie++)
					{
						unsigned int no; es.GetNodes(ie, &no);
						double C[3]; ns_c.GetValue(no, C);
						double u[3]; ns_u.GetValue(no, u);
						double c[3] = { C[0] + u[0], C[1] + u[1], C[2] + u[2] };
						::glVertex3dv(c);
					}
					::glEnd();
				}
			}
		}
		/*
		if( itype_hilight_ == Cad::LOOP ){
		::glColor3d(1,1,0);
		::glBegin(GL_TRIANGLES);
		for(unsigned int ie=0;ie<es.Size();ie++){
		unsigned int no[3]; es.GetNodes(ie,no);
		double C[3][3]; ns_c.GetValue(no[0],C[0]); ns_c.GetValue(no[1],C[1]); ns_c.GetValue(no[2],C[2]);
		double u[3][3]; ns_u.GetValue(no[0],u[0]); ns_u.GetValue(no[1],u[1]); ns_u.GetValue(no[2],u[2]);
		double c[3][3] = {
		{C[0][0]+u[0][0],C[0][1]+u[0][1],C[0][2]+u[0][2]},
		{C[1][0]+u[1][0],C[1][1]+u[1][1],C[1][2]+u[1][2]},
		{C[2][0]+u[2][0],C[2][1]+u[2][1],C[2][2]+u[2][2]} };
		::glVertex3dv(c[0]);
		::glVertex3dv(c[1]);
		::glVertex3dv(c[2]);
		}
		::glEnd();
		}
		}
		*/
		if (is_lighting) { glEnable(GL_LIGHTING); }
		if (is_texture) { glEnable(GL_TEXTURE_2D); }
		::glDisable(GL_LINE_SMOOTH);
		::glDisable(GL_BLEND);
	}
	//  if( imode_ == SIMULATION_DETAIL || imode_ == SIMULATION_DETAIL_CONVERGED ){ m_aDrawerField_detail.Draw(); }
	//  else{ m_aDrawerField.Draw(); }
	m_aDrawerField.Draw();
	m_aDrawerField_detail.Draw();
	////
	stitch_ary_.Draw(id_field_disp, world);
	////

	if (obj_mesh.IsEmpty())
	{
		if (this->imode_ == CLOTH_INITIAL_LOCATION) { clothHandler_.Draw(1); }
		else { clothHandler_.Draw(0); }
	}
	else
	{
		obj_mesh.Draw();
	}
}

void CAnalysis2D_Cloth_Static::MoveClothLoopInitialPosition
(unsigned int id_l, double der[3])
{
	unsigned int id_ea = 0;
	{
		const Fem::Field::CIDConvEAMshCad& conv = world.GetIDConverter(id_field_base);
		id_ea = conv.GetIdEA_fromCad(id_l, Cad::LOOP);
	}
	double p[3], n[3], h[3];
	if (!clothHandler_.GetAnchor_3D(p, n, h, id_ea)) { return; }
	const double anc_x = p[0] + der[0];
	const double anc_y = p[1] + der[1];
	const double anc_z = p[2] + der[2];
	clothHandler_.Transform_Cloth_Pan(id_l, anc_x, anc_y, anc_z);
	if (imode_ == CLOTH_INITIAL_LOCATION)
	{
		//    std::cout << anc_x << " " << anc_y << " " << anc_z << std::endl;
		clothHandler_.SetClothLocation(id_field_disp, world);
		m_aDrawerField.Update(world);
		if (is_detail_)
		{
			assert(id_field_disp_detail != 0);
			assert(id_field_base_detail != 0);
			MoveFineDeformedCoord(id_field_disp_detail, id_field_base_detail,
				id_field_disp, id_field_base,
				world, aInterp_detail);
			m_aDrawerField_detail.Update(world);
		}
	}
}

void CAnalysis2D_Cloth_Static::RotateClothLoopInitialPosition(unsigned int id_l, const ldp::Mat3d& R)
{
	const Fem::Field::CIDConvEAMshCad& conv = world.GetIDConverter(id_field_base);
	unsigned int id_ea = conv.GetIdEA_fromCad(id_l, Cad::LOOP);

	ldp::Double3 p, n, h;
	if (!clothHandler_.GetAnchor_3D(p.ptr(), n.ptr(), h.ptr(), id_ea)) { return; }
	clothHandler_.Transform_Cloth_Rot(id_l, R, false);
	if (imode_ == CLOTH_INITIAL_LOCATION)
	{
		//    std::cout << anc_x << " " << anc_y << " " << anc_z << std::endl;
		clothHandler_.SetClothLocation(id_field_disp, world);
		m_aDrawerField.Update(world);
		if (is_detail_)
		{
			assert(id_field_disp_detail != 0);
			assert(id_field_base_detail != 0);
			MoveFineDeformedCoord(id_field_disp_detail, id_field_base_detail,
				id_field_disp, id_field_base,
				world, aInterp_detail);
			m_aDrawerField_detail.Update(world);
		}
	}
}


void CAnalysis2D_Cloth_Static::DrawBoundaryCondition(const Cad::CCadObj2D& cad_2d) const
{
	stitch_ary_.DrawBoundaryCondition2D(cad_2d);

	if (imode_ == CLOTH_INITIAL_LOCATION)
	{
		bool is_lighting = ::glIsEnabled(GL_LIGHTING);
		::glDisable(GL_LIGHTING);
		bool is_texture = ::glIsEnabled(GL_TEXTURE_2D);
		::glDisable(GL_TEXTURE_2D);
		if (!world.IsIdField(id_field_disp)) return;
		const Fem::Field::CField& field_disp = world.GetField(id_field_disp);
		const std::vector<unsigned int>& aIdEA = field_disp.GetAryIdEA();
		for (unsigned int iiea = 0; iiea < aIdEA.size(); iiea++)
		{
			unsigned int id_ea = aIdEA[iiea];
			double r[2];
			if (clothHandler_.GetAnchor_2D(r, id_ea))
			{
				::glPointSize(10);
				::glColor3d(0, 1, 0);
				::glBegin(GL_POINTS);
				::glVertex2d(r[0], r[1]);
				::glEnd();
				::glColor3d(1, 0, 0);
				::glBegin(GL_LINES);
				::glVertex2d(r[0], r[1]);
				::glVertex2d(r[0] + 0.3, r[1]);
				::glEnd();
			}
		}
		if (is_lighting) { ::glEnable(GL_LIGHTING); }
		if (is_texture) { ::glEnable(GL_TEXTURE_2D); }

	}
}


void CAnalysis2D_Cloth_Static::GuessSolutionSliderMove(double pre_v, double pos_v)
{
	//  mouse_obj_x = pos_x;  // use in Solve() to measure distance from sampling points
	//  mouse_obj_y = pos_y;
	mouse_slider_v = pos_v;
	// imode_sensitivity_guess_ :  (0)upd, (1)sens, (2)sens+upd, (3)gmls
	if (this->imode_sensitivity_guess_ == 0)
	{
		double dist_v = pos_v - pre_v;
		NoResponse_Slider
			(dist_v,
			id_field_disp,
			id_field_disp_buffer,  // don't change deformed position
			id_field_lamX,
			world);
	}
	else if (this->imode_sensitivity_guess_ == 1 || imode_sensitivity_guess_ == 2)
	{
		double dist_v = pos_v - pre_v;
		const unsigned int id_field_senseX = aSolSens[0].id_field_dudpx;
		SensitiveResponse_Slider
			(dist_v,
			id_field_disp,
			id_field_disp_buffer,  // don't change deformed position
			id_field_lamX,
			id_field_senseX,
			world);
	}
	else if (this->imode_sensitivity_guess_ == 3 || this->imode_sensitivity_guess_ == 4)
	{
		GuessSolution_GMLS_Slider(pre_v, pos_v,
			id_field_disp,
			id_field_disp_buffer,
			id_field_lamX,
			aSolSens,
			world);
	}
	else if (this->imode_sensitivity_guess_ == 5)
	{
		/*
		unsigned int icnt=0;
		for(unsigned int iss=0;iss<aSolSens.size();iss++){
		if( aSolSens[iss].is_active ){ icnt++; }
		}
		if( icnt < 1 ){
		double dist_v = pos_v - pre_v;
		NoResponse_Slider
		(dist_v,
		id_field_disp,
		id_field_disp_buffer,  // don't change deformed position
		id_field_lamX,
		world );
		}
		else{
		GuessSolution_MLS_Slider(pre_v, pos_v,
		id_field_disp,
		id_field_disp_buffer,
		id_field_lamX,
		aSolSens,
		world);
		}
		*/
	}
	{
		Fem::Field::CField& field_disp = world.GetField(id_field_disp);
		Fem::Field::CNodeAry::CNodeSeg& ns_v = field_disp.GetNodeSeg(CORNER, true, world, VELOCITY);
		for (unsigned int ino = 0; ino < ns_v.Size(); ino++)
		{
			ns_v.SetValue(ino, 0, 0);
			ns_v.SetValue(ino, 1, 0);
			ns_v.SetValue(ino, 2, 0);
		}
	}
	////
	m_aDrawerField.Update(world);
	if (is_detail_)
	{
		assert(id_field_disp_detail != 0);
		assert(id_field_base_detail != 0);
		MoveFineDeformedCoord(id_field_disp_detail, id_field_base_detail,
			id_field_disp, id_field_base,
			world, aInterp_detail);
		m_aDrawerField_detail.Update(world);
	}
}

void CAnalysis2D_Cloth_Static::GuessSolutionMouseMove(double pre_x, double pre_y,
	double pos_x, double pos_y)
{
	///  std::cout << "GuessSolutionMouseMove" << imode_ << std::endl;
	mouse_obj_x = pos_x;
	mouse_obj_y = pos_y;
	if (imode_ != SENSITIVITY_DONE)
	{
		double dist_x = pos_x - pre_x;
		double dist_y = pos_y - pre_y;
		NoResponse
			(dist_x, dist_y,
			id_field_disp,
			id_field_disp_buffer,  // don't change deformed position
			id_field_lamX, id_field_lamY,
			world);
		return;
	}
	// imode_sensitivity_guess_ :  (0)upd, (1)sens, (2)sens+upd, (3)gmls
	if (this->imode_sensitivity_guess_ == 0)
	{
		double dist_x = pos_x - pre_x;
		double dist_y = pos_y - pre_y;
		NoResponse
			(dist_x, dist_y,
			id_field_disp,
			id_field_disp_buffer,  // don't change deformed position
			id_field_lamX, id_field_lamY,
			world);
	}
	else if (this->imode_sensitivity_guess_ == 1 || imode_sensitivity_guess_ == 2)
	{
		double dist_x = pos_x - pre_x;
		double dist_y = pos_y - pre_y;
		const unsigned int id_field_senseX = aSolSens[0].id_field_dudpx;
		const unsigned int id_field_senseY = aSolSens[0].id_field_dudpy;
		SensitiveResponse
			(dist_x, dist_y,
			id_field_disp,
			id_field_disp_buffer,  // don't change deformed position
			id_field_lamX, id_field_lamY,
			id_field_senseX, id_field_senseY,
			world);
	}
	else if (this->imode_sensitivity_guess_ == 3 || this->imode_sensitivity_guess_ == 4)
	{
		GuessSolution_GMLS(pre_x, pre_y, pos_x, pos_y,
			id_field_disp,
			id_field_disp_buffer,
			id_field_lamX, id_field_lamY,
			aSolSens,
			world);
	}
	else if (this->imode_sensitivity_guess_ == 5)
	{
		unsigned int icnt = 0;
		for (unsigned int iss = 0; iss < aSolSens.size(); iss++)
		{
			if (aSolSens[iss].is_active) { icnt++; }
		}
		if (icnt < 1)
		{
			double dist_x = pos_x - pre_x;
			double dist_y = pos_y - pre_y;
			NoResponse
				(dist_x, dist_y,
				id_field_disp,
				id_field_disp_buffer,  // don't change deformed position
				id_field_lamX, id_field_lamY,
				world);
			/*
				  GuessSolution_GMLS(pre_x, pre_y, pos_x, pos_y,
				  id_field_disp,
				  id_field_disp_buffer,
				  id_field_lamX, id_field_lamY,
				  aSolSens,
				  world);
				  */
		}
		else
		{
			//      std::cout << "mls" << std::endl;
			GuessSolution_MLS(pre_x, pre_y, pos_x, pos_y,
				id_field_disp,
				id_field_disp_buffer,
				id_field_lamX, id_field_lamY,
				aSolSens,
				world);
		}
	}
	{
		Fem::Field::CField& field_disp = world.GetField(id_field_disp);
		Fem::Field::CNodeAry::CNodeSeg& ns_v = field_disp.GetNodeSeg(CORNER, true, world, VELOCITY);
		for (unsigned int ino = 0; ino < ns_v.Size(); ino++)
		{
			ns_v.SetValue(ino, 0, 0);
			ns_v.SetValue(ino, 1, 0);
			ns_v.SetValue(ino, 2, 0);
		}
	}
	////
	m_aDrawerField.Update(world);
	if (is_detail_)
	{
		assert(id_field_disp_detail != 0);
		assert(id_field_base_detail != 0);
		MoveFineDeformedCoord(id_field_disp_detail, id_field_base_detail,
			id_field_disp, id_field_base,
			world, aInterp_detail);
		m_aDrawerField_detail.Update(world);
	}
}

void CAnalysis2D_Cloth_Static::Update_Boundary_Condition_Cad_Move
(Cad::CAD_ELEM_TYPE itype, unsigned int cad_elem_id,
double del_x, double del_y,
CSliderDeform& slider_deform)
{
	if (itype == Cad::LOOP)
	{
		const Fem::Field::CIDConvEAMshCad& conv = world.GetIDConverter(id_field_base);
		unsigned int id_ea = conv.GetIdEA_fromCad(cad_elem_id, itype);
		clothHandler_.MoveAnchor_2D(del_x, del_y, id_ea);
		double r[2];
		unsigned int id_l = cad_elem_id;
		clothHandler_.GetAnchor_2D_Loop(r, id_l);
		slider_deform.SetLoopCenter(id_l, r[0], r[1]);
	}
}

SOLVER_FLAG CAnalysis2D_Cloth_Static::UpdateMeshAndSolve
(const Msh::CMesher2D& mesh_2d,
bool is_updated_coord, bool is_updated_edge,
bool is_solve_upd_mesh, bool is_update_disp)
{
	if (is_updated_coord || is_updated_edge)
	{
		if (is_updated_coord)
		{
			is_updated_coord = false;
			if (is_update_disp)
			{
				world.UpdateMeshCoord(id_field_base, id_field_disp, Msh::CMeshProjector2Dto3D(mesh_2d));
			}
			else
			{
				world.UpdateMeshCoord(id_field_base, Msh::CMeshProjector2Dto3D(mesh_2d));
			}
			if (is_detail_)
			{
				assert(id_field_base_detail != 0);
				MoveFineBaseCoord(id_field_base_detail, id_field_base, world, aInterp_detail);
				if (is_update_disp && imode_ != CLOTH_INITIAL_LOCATION)
				{
					MoveFineDeformedCoord(id_field_disp_detail, id_field_base_detail,
						id_field_disp, id_field_base,
						world, aInterp_detail);
				}
			}
		}
		if (is_updated_edge)
		{
			is_updated_edge = false;
			world.UpdateConnectivity(id_field_base, mesh_2d);
			world.UpdateConnectivity_HingeField_Tri(id_field_hinge, id_field_disp);
			ClearLinearSystemPreconditioner();
			if (imode_ == CLOTH_INITIAL_LOCATION)
			{
				clothHandler_.BuildClothMeshTopology(id_field_base, id_field_disp, world);
			}
			if (is_detail_)
			{
				assert(id_field_base_detail != 0);
				FindBaseInterp(id_field_base_detail, id_field_base, world, aInterp_detail); // this somethime fails
			}
		}
		if (imode_ == CLOTH_INITIAL_LOCATION)
		{
			clothHandler_.SetClothLocation(id_field_disp, world);
			if (is_detail_)
			{
				assert(id_field_disp_detail != 0);
				assert(id_field_base_detail != 0);
				MoveFineDeformedCoord(id_field_disp_detail, id_field_base_detail,
					id_field_disp, id_field_base,
					world, aInterp_detail);
			}
		}
		this->InitDrawer();	// ï`âÊîzóÒÇçÏÇËíºÇ∑
		//    std::cout << "updated  fdasfd" << std::endl;
	}
	if (imode_ != CLOTH_INITIAL_LOCATION && is_solve_upd_mesh)
	{
		//    std::cout << "SolveW" << std::endl;
		this->Solve();
		cur_time_ += dt_;
	}
	return SUCCESS;
}

void CAnalysis2D_Cloth_Static::BuildFEM_ClearValueField(const Cad::CCadObj2D& cad_2d, const Msh::CMesher2D& mesh_2d)
{
	this->InitFieldEqn_fromMsh(cad_2d, mesh_2d);
	this->InitDrawer();
	this->ClearLinearSystemPreconditioner();
	clothHandler_.SetClothLocation(id_field_disp, world);
	CopyValueVelo(id_field_disp_buffer, id_field_disp, world);
}

void CAnalysis2D_Cloth_Static::BuildFEM_InterpValueField(const Cad::CCadObj2D& cad_2d, const Msh::CMesher2D& mesh_2d,
	const std::vector< std::pair<unsigned int, unsigned int> >& aNewL)
{
	{
		for (unsigned int i = 0; i < aNewL.size(); i++)
		{
			clothHandler_.AddClothPiece(aNewL[i].first, aNewL[i].second);
		}
	}
	if (this->imode_ == CLOTH_INITIAL_LOCATION)
	{
		this->BuildFEM_ClearValueField(cad_2d, mesh_2d);
		return;
	}
	this->ClearLinearSystemPreconditioner();
	////////
	////////
	unsigned int id_field_base_old = id_field_base;
	unsigned int id_field_disp_old = id_field_disp;
	id_field_base = world.AddMesh(Msh::CMeshProjector2Dto3D(mesh_2d));
	id_field_disp = world.MakeField_FieldElemDim(id_field_base, 2, VECTOR3, VALUE | VELOCITY, CORNER);
	InterpField(id_field_disp, id_field_base, id_field_disp_old, id_field_base_old, world);
	/////////////////////////////////
	{
		std::vector<unsigned int> aIdFieldDel;
		aIdFieldDel.push_back(id_field_base_old);
		aIdFieldDel.push_back(id_field_disp_old);
		aIdFieldDel.push_back(id_field_disp_buffer);
		aIdFieldDel.push_back(id_field_hinge);
		for (unsigned int iss = 0; iss < aSolSens.size(); iss++)
		{
			unsigned int id_field_dudpx = aSolSens[iss].id_field_dudpx;
			unsigned int id_field_dudpy = aSolSens[iss].id_field_dudpy;
			unsigned int id_field_x = aSolSens[iss].id_field_x;
			if (world.IsIdField(id_field_dudpx)) { aIdFieldDel.push_back(id_field_dudpx); }
			if (world.IsIdField(id_field_dudpy)) { aIdFieldDel.push_back(id_field_dudpy); }
			if (world.IsIdField(id_field_x)) { aIdFieldDel.push_back(id_field_x); }
		}
		aIdFieldDel.push_back(id_field_lamX);
		aIdFieldDel.push_back(id_field_lamY);
		const std::vector<unsigned int>& aIdFieldStitch = this->stitch_ary_.GetIdFieldAry();
		for (unsigned int ist = 0; ist < aIdFieldStitch.size(); ist++)
		{
			aIdFieldDel.push_back(aIdFieldStitch[ist]);
		}
		world.DeleteField(aIdFieldDel);
		id_field_hinge = 0;
		for (unsigned int iss = 0; iss < aSolSens.size(); iss++)
		{
			aSolSens[iss].id_field_dudpx = 0;
			aSolSens[iss].id_field_dudpy = 0;
			aSolSens[iss].id_field_x = 0;
			aSolSens[iss].is_active = false;
		}
		imode_ = SIMULATION_STATIC;
		id_field_lamX = 0;
		id_field_lamY = 0;
		this->aFrictionPoint.clear();
	}
	if (is_detail_)
	{
		this->ClearDetailField();
		is_detail_ = true;
	}
	else
	{
		is_detail_ = false;
	}
	id_field_disp_buffer = world.MakeField_FieldElemDim(id_field_base, 2, VECTOR3, VALUE | VELOCITY, CORNER);
	id_field_hinge = MakeHingeField_Tri(world, id_field_disp);
	////  
	for (unsigned int iss = 0; iss < aSolSens.size(); iss++)
	{
		unsigned int id_field_dudpx = world.MakeField_FieldElemDim(id_field_base, 2, VECTOR3, VALUE, CORNER);
		unsigned int id_field_dudpy = world.MakeField_FieldElemDim(id_field_base, 2, VECTOR3, VALUE, CORNER);
		unsigned int id_field_x = world.MakeField_FieldElemDim(id_field_base, 2, VECTOR3, VALUE, CORNER);
		aSolSens[iss].id_field_dudpx = id_field_dudpx;
		aSolSens[iss].id_field_dudpy = id_field_dudpy;
		aSolSens[iss].id_field_x = id_field_x;
		aSolSens[iss].is_active = false;
	}
	id_field_lamX = world.MakeField_FieldElemDim(id_field_base, 2, VECTOR2, VALUE, CORNER);
	id_field_lamY = world.MakeField_FieldElemDim(id_field_base, 2, VECTOR2, VALUE, CORNER);
	////
	{
		const Fem::Field::CField& field_disp = world.GetField(id_field_disp);
		const CNodeAry::CNodeSeg& ns_co = field_disp.GetNodeSeg(CORNER, false, world, VALUE);
		const unsigned int nno = ns_co.Size();
		aFrictionPoint.clear();
		aFrictionPoint.resize(nno);
	}
	stitch_ary_.MakeField(id_field_disp, id_field_base, world);
	{
		aIdField_Fix.clear();
		const Fem::Field::CIDConvEAMshCad& conv = world.GetIDConverter(id_field_base);
		for (unsigned int iide = 0; iide < aIdECad_Fix.size(); iide++)
		{
			const unsigned int id_e = aIdECad_Fix[iide];
			aIdField_Fix.push_back(world.GetPartialField(id_field_disp, conv.GetIdEA_fromCad(id_e, Cad::EDGE)));
		}
	}
	this->ClearLinearSystemPreconditioner();


	clothHandler_.BuildClothMeshTopology(id_field_base, id_field_disp, world);

	CopyValueVelo(id_field_disp_buffer, id_field_disp, world);

	////////////////

	if (is_detail_)
	{
		std::cout << "Interp FEM field detail" << std::endl;
		MakeDetailField(cad_2d, mesh_2d);
	}
	else
	{
		std::cout << "Interp FEM field no-detail" << std::endl;
		id_field_base_detail = 0;
		id_field_disp_detail = 0;
		id_field_disp_buffer_detail = 0;
		id_field_hinge_detail = 0;
		stitch_ary_detail.Clear();
		aFrictionPoint_detail.clear();
		aInterp_detail.clear();
	}

	this->InitDrawer();
	this->ClearLinearSystemPreconditioner();
}


SOLVER_FLAG CAnalysis2D_Cloth_Static::Solve()
{
	if (imode_sensitivity_guess_ == 1 && imode_ == SENSITIVITY_DONE)
	{
		return SUCCESS;
	}
	if (this->is_cleared_ls_prec)
	{
		std::cout << "Solve::Update Prec LS" << std::endl;
		if (imode_ == SIMULATION_DETAIL)
		{
			assert(is_detail_);
			ls.AddPattern_Field(id_field_hinge_detail, world);
			for (unsigned int iiff = 0; iiff < aIdField_Fix_detail.size(); iiff++)
			{
				const unsigned int id_field0 = aIdField_Fix_detail[iiff];
				ls.SetFixedBoundaryCondition_Field(id_field0, world);
			}
			stitch_ary_detail.AddPatternLinearSystem(ls, world);
			stitch_ary_detail.SetReorderingPreconditionerIfNeeded(prec, id_field_disp_detail, world);
		}
		else
		{
			ls.AddPattern_Field(id_field_hinge, world);
			for (unsigned int iiff = 0; iiff < aIdField_Fix.size(); iiff++)
			{
				const unsigned int id_field0 = aIdField_Fix[iiff];
				ls.SetFixedBoundaryCondition_Field(id_field0, world);
			}
			stitch_ary_.AddPatternLinearSystem(ls, world);
			stitch_ary_.SetReorderingPreconditionerIfNeeded(prec, id_field_disp, world);
		}
		prec.SetFillInLevel(0);
		prec.SetLinearSystem(ls.m_ls);
		this->is_cleared_ls_prec = false;
	}
	if (imode_ == SIMULATION_STATIC || imode_ == SENSITIVITY_DONE)
	{
		/*    if( SENSITIVITY_DONE ){
			  unsigned int icnt=0;
			  for(unsigned int iss=0;iss<aSolSens.size();iss++){
			  if( aSolSens[iss].is_active ){ icnt++; }
			  }
			  if( icnt == 2 ){
			  return SUCCESS;
			  }
			  }*/
		bool is_ilufrac_success = true;
		bool res = StepTime_Static
			(dt_,
			total_energy_,
			2.0e-4, //cloth
			is_ilufrac_success,
			//     1.0e-5,     
			ls, prec,
			cloth_param,
			gx_, gy_, gz_,
			////
			contact_param,
			*pCT,
			aFrictionPoint,
			////
			stitch_ary_,
			////
			id_field_disp, id_field_hinge,
			id_field_disp_buffer,
			world);
		//    cur_time_ += dt_;    
		if (total_energy_ref < 0) { total_energy_ref = total_energy_; }
		//    std::cout << "Eneargy " << total_energy_ << " " << total_energy_ref << std::endl;
		if (is_detail_)
		{
			MoveFineDeformedCoord(id_field_disp_detail, id_field_base_detail,
				id_field_disp, id_field_base,
				world,
				aInterp_detail);
			m_aDrawerField_detail.Update(world);
		}
		m_aDrawerField.Update(world);
		if (res && imode_ == SENSITIVITY_DONE)
		{
			if (imode_sensitivity_guess_ != 4 && imode_sensitivity_guess_ != 5) return SUCCESS;
			// imode_sensitivity_guess_ == 4 : prg+gmls
			const bool is_xy0 = aSolSens[0].is_xy;
			for (unsigned int iss = 0; iss < aSolSens.size(); iss++)
			{
				if (!aSolSens[iss].is_active) continue;
				if (is_xy0)
				{
					const double obj_x = aSolSens[iss].obj_x;
					const double obj_y = aSolSens[iss].obj_y;
					const double sqdist = (obj_x - mouse_obj_x)*(obj_x - mouse_obj_x) + (obj_y - mouse_obj_y)*(obj_y - mouse_obj_y);
					if (sqdist < 0.01) return SUCCESS;
				}
				else
				{
					const double obj_v = aSolSens[iss].val_slider;
					const double sqdist = (obj_v - mouse_slider_v)*(obj_v - mouse_slider_v);
					if (sqdist < 0.01) return SUCCESS;
				}
			}
			int iss0 = -1;
			for (unsigned int iss = 1; iss < aSolSens.size(); iss++)
			{
				if (aSolSens[iss].is_active == false) { iss0 = iss; break; }
			}
			if (iss0 == -1)
			{
				double sqdist_max = 0;
				unsigned int iss_md = 0;
				for (unsigned int iss = 1; iss<aSolSens.size(); iss++)
				{
					if (is_xy0)
					{
						const double obj_x = aSolSens[iss].obj_x;
						const double obj_y = aSolSens[iss].obj_y;
						const double sqdist = (obj_x - mouse_obj_x)*(obj_x - mouse_obj_x) + (obj_y - mouse_obj_y)*(obj_y - mouse_obj_y);
						if (sqdist > sqdist_max)
						{
							iss_md = iss;
							sqdist_max = sqdist;
						}
					}
					else
					{
						const double obj_v = aSolSens[iss].val_slider;
						const double sqdist = (obj_v - mouse_slider_v)*(obj_v - mouse_slider_v);
						if (sqdist > sqdist_max)
						{
							iss_md = iss;
							sqdist_max = sqdist;
						}
					}
				}
				iss0 = iss_md;
			}
			std::cout << "set middle sensitivity" << std::endl;
			std::cout << "<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>" << std::endl;
			//      std::cout << mouse_obj_x << " " << mouse_obj_y << std::endl;
			aSolSens[iss0].is_active = true;
			if (is_xy0)
			{
				aSolSens[iss0].is_xy = true;
				aSolSens[iss0].obj_x = mouse_obj_x;
				aSolSens[iss0].obj_y = mouse_obj_y;
			}
			else
			{
				aSolSens[iss0].is_xy = false;
				aSolSens[iss0].val_slider = mouse_slider_v;
			}
			unsigned int id_field_senseX = aSolSens[iss0].id_field_dudpx;
			unsigned int id_field_senseY = aSolSens[iss0].id_field_dudpy;
			unsigned int id_field_x = aSolSens[iss0].id_field_x;
			std::cout << id_field_x << " " << iss0 << std::endl;
			assert(world.IsIdField(id_field_x));
			SetDeformedValue(id_field_x, id_field_disp, world);
			//  GetSensitivity      
			GetSensitivity_fictbend
				(ls, prec,
				////
				cloth_param,
				gx_, gy_, gz_,
				////
				contact_param,
				*pCT,
				aFrictionPoint,
				/////
				stitch_ary_,
				/////
				id_field_disp,
				id_field_hinge,
				/////
				is_xy0,
				id_field_senseX, id_field_senseY,
				id_field_lamX, id_field_lamY,
				world);
			CopyValueVelo(id_field_disp_buffer, id_field_disp, world);
		}
		if (res && imode_ == SIMULATION_STATIC)
		{
			total_energy_ref = -1;
			if (is_detail_)
			{
				imode_ = SIMULATION_DETAIL;
				this->ClearLinearSystemPreconditioner();
			}
			else
			{
				imode_ = WAITING_PICK;
			}
		}
	}
	else if (imode_ == SIMULATION_DETAIL)
	{
		assert(is_detail_);
		bool is_ilufrac_succes = true;
		bool res = StepTime_Static
			(dt_,
			total_energy_,
			1.0e-4,
			is_ilufrac_succes,
			ls, prec,
			cloth_param,
			gx_, gy_, gz_,
			////
			contact_param,
			*pCT,
			aFrictionPoint_detail,
			////
			stitch_ary_detail,
			////
			id_field_disp_detail, id_field_hinge_detail,
			id_field_disp_buffer_detail,
			world);
		if (total_energy_ref < 0) { total_energy_ref = total_energy_; }
		m_aDrawerField_detail.Update(world);
		UpdateFineDeformedInterp(id_field_disp_detail, id_field_base_detail,
			id_field_disp, id_field_base,
			world,
			aInterp_detail);
		//    std::cout << "Eneargy" << total_energy_ << " " << total_energy_ref << std::endl;          
		if (res)
		{
			total_energy_ref = -1;
			imode_ = WAITING_PICK;
			this->ClearLinearSystemPreconditioner();
		}
	}

	return SUCCESS;
}

void CAnalysis2D_Cloth_Static::ClearLinearSystemPreconditioner()
{
	std::cout << "Clear Linear System Prec" << std::endl;
	ls.Clear();
	prec.Clear();
	this->is_cleared_ls_prec = true;
}

void CAnalysis2D_Cloth_Static::InitFieldEqn_fromMsh(const Cad::CCadObj2D& cad_2d, const Msh::CMesher2D& mesh_2d)
{
	std::cout << "Init Field Eqn from Mesh " << std::endl;
	//  assert( imode_ != SIMULATION_DETAIL );
	world.Clear();
	id_field_base = world.AddMesh(Msh::CMeshProjector2Dto3D(mesh_2d));
	std::cout << id_field_base << std::endl;
	//	const Fem::Field::CIDConvEAMshCad& conv = world.GetIDConverter(id_field_base);
	id_field_disp = world.MakeField_FieldElemDim(id_field_base, 2, VECTOR3, VALUE | VELOCITY, CORNER);
	id_field_disp_buffer = world.MakeField_FieldElemDim(id_field_base, 2, VECTOR3, VALUE | VELOCITY, CORNER);
	id_field_hinge = MakeHingeField_Tri(world, id_field_disp);
	////
	for (unsigned int iss = 0; iss < aSolSens.size(); iss++)
	{
		unsigned int id_field_dudpx = world.MakeField_FieldElemDim(id_field_base, 2, VECTOR3, VALUE, CORNER);
		unsigned int id_field_dudpy = world.MakeField_FieldElemDim(id_field_base, 2, VECTOR3, VALUE, CORNER);
		unsigned int id_field_x = world.MakeField_FieldElemDim(id_field_base, 2, VECTOR3, VALUE, CORNER);
		aSolSens[iss].id_field_dudpx = id_field_dudpx;
		aSolSens[iss].id_field_dudpy = id_field_dudpy;
		aSolSens[iss].id_field_x = id_field_x;
		aSolSens[iss].is_active = false;
	}
	id_field_lamX = world.MakeField_FieldElemDim(id_field_base, 2, VECTOR2, VALUE, CORNER);
	id_field_lamY = world.MakeField_FieldElemDim(id_field_base, 2, VECTOR2, VALUE, CORNER);
	////
	{
		const Fem::Field::CField& field_disp = world.GetField(id_field_disp);
		const CNodeAry::CNodeSeg& ns_co = field_disp.GetNodeSeg(CORNER, false, world, VALUE);
		const unsigned int nno = ns_co.Size();
		aFrictionPoint.clear();
		aFrictionPoint.resize(nno);
	}
	stitch_ary_.MakeField(id_field_disp, id_field_base, world);
	this->ClearLinearSystemPreconditioner();

	clothHandler_.BuildClothMeshTopology(id_field_base, id_field_disp, world);
	clothHandler_.SetClothLocation(id_field_disp, world);
	CopyValueVelo(id_field_disp_buffer, id_field_disp, world);

	{
		aIdField_Fix.clear();
		const Fem::Field::CIDConvEAMshCad& conv = world.GetIDConverter(id_field_base);
		for (unsigned int iide = 0; iide < aIdECad_Fix.size(); iide++)
		{
			const unsigned int id_e = aIdECad_Fix[iide];
			aIdField_Fix.push_back(world.GetPartialField(id_field_disp, conv.GetIdEA_fromCad(id_e, Cad::EDGE)));
		}
	}

	////////////////

	if (is_detail_)
	{
		MakeDetailField(cad_2d, mesh_2d);
	}
	else
	{
		id_field_base_detail = 0;
		id_field_disp_detail = 0;
		id_field_disp_buffer_detail = 0;
		id_field_hinge_detail = 0;
		stitch_ary_detail.Clear();
		aFrictionPoint_detail.clear();
		aInterp_detail.clear();
	}
	////

	cur_time_ = 0;
	dt_ = 0.005;
}

void CAnalysis2D_Cloth_Static::SetHilight(Cad::CAD_ELEM_TYPE itype, unsigned int cad_elem_id)
{
	this->itype_hilight_ = itype;
	this->id_hilight_ = cad_elem_id;
}

void CAnalysis2D_Cloth_Static::ConnectEdge(const Cad::CCadObj2D& cad_2d, const Msh::CMesher2D& mesh_2d,
	unsigned int id_e1, unsigned int id_e2)
{
	//  std::cout << "id_bse " << id_field_base << " " << world.IsIdField(id_field_base) << std::endl;
	stitch_ary_.AddStitchField(cad_2d, mesh_2d, id_e1, id_e2, id_field_disp, id_field_base, world);
	if (is_detail_)
	{
		stitch_ary_detail.AddStitchField(cad_2d, mesh_2d, id_e1, id_e2, id_field_disp_detail, id_field_base_detail, world);
	}
	this->ClearLinearSystemPreconditioner();
}

void CAnalysis2D_Cloth_Static::DisconnectEdge(unsigned int id_e1, unsigned int id_e2)
{
	//  std::cout << "id_bse " << id_field_base << " " << world.IsIdField(id_field_base) << std::endl;
	stitch_ary_.RemoveStitchField(id_e1, id_e2, id_field_disp, id_field_base, world);
	if (is_detail_)
	{
		stitch_ary_detail.RemoveStitchField(id_e1, id_e2, id_field_disp_detail, id_field_base_detail, world);
	}
	this->ClearLinearSystemPreconditioner();
}

bool CAnalysis2D_Cloth_Static::IsSeamLine(unsigned int id_e_in, unsigned int& id_e_out, bool& is_same_dir) const
{
	return this->stitch_ary_.IsSeamLine(id_e_in, id_e_out, is_same_dir);
}

void CAnalysis2D_Cloth_Static::SetSensitivity_Slider
(unsigned int islider,
const std::vector<double>& har,
double val)
{
	if (imode_ == SIMULATION_DETAIL)
	{
		this->ClearLinearSystemPreconditioner();
		imode_ = WAITING_PICK;
	}
	//  this->ClearLinearSystemPreconditioner();      
	//  if( imode_ != WAITING_PICK ) return;
	if (this->is_cleared_ls_prec)
	{
		std::cout << "SetSensitivity_Slider::Update Prec LS" << std::endl;
		ls.AddPattern_Field(id_field_hinge, world);
		stitch_ary_.AddPatternLinearSystem(ls, world);
		for (unsigned int iiff = 0; iiff < aIdField_Fix.size(); iiff++)
		{
			const unsigned int id_field0 = aIdField_Fix[iiff];
			ls.SetFixedBoundaryCondition_Field(id_field0, world);
		}
		////
		stitch_ary_.SetReorderingPreconditionerIfNeeded(prec, id_field_disp, world);
		prec.SetFillInLevel(0);
		prec.SetLinearSystem(ls.m_ls);
	}
	{
		Fem::Field::CField& field_lx = world.GetField(id_field_lamX);
		Fem::Field::CNodeAry::CNodeSeg& ns_lx = field_lx.GetNodeSeg(CORNER, true, world, VALUE);
		const unsigned int nnode = ns_lx.Size();
		assert(har.size() == nnode * 4);
		for (unsigned int ino = 0; ino < nnode; ino++)
		{
			ns_lx.SetValue(ino, 0, har[ino * 4 + 0]);
			ns_lx.SetValue(ino, 1, har[ino * 4 + 2]);
		}
	}
	if (imode_sensitivity_guess_ == 0)
	{
		imode_ = SENSITIVITY_DONE;
		return;
	}
	for (unsigned int iss = 0; iss < aSolSens.size(); iss++) { aSolSens[iss].is_active = false; }
	aSolSens[0].is_active = true;
	aSolSens[0].is_xy = false;
	aSolSens[0].val_slider = val;
	unsigned int id_field_senseX = aSolSens[0].id_field_dudpx;
	unsigned int id_field_senseY = aSolSens[0].id_field_dudpy;
	unsigned int id_field_x = aSolSens[0].id_field_x;
	SetDeformedValue(id_field_x, id_field_disp, world);
	//  GetSensitivity
	bool is_sensitivity_solved = GetSensitivity_fictbend
		(ls, prec,
		////
		cloth_param,
		gx_, gy_, gz_,
		////
		contact_param,
		*pCT,
		aFrictionPoint,
		/////
		stitch_ary_,
		/////
		id_field_disp,
		id_field_hinge,
		false,
		id_field_senseX, id_field_senseY,
		id_field_lamX, id_field_lamY,
		world);
	CopyValueVelo(id_field_disp_buffer, id_field_disp, world);
	if (is_sensitivity_solved) { imode_ = SENSITIVITY_DONE; }

}


void CAnalysis2D_Cloth_Static::SetSensitivity(Cad::CAD_ELEM_TYPE itype_cad_part, unsigned int id_cad_part,
	const std::vector<double>& har,
	double obj_x, double obj_y)
{
	if (imode_ == SIMULATION_DETAIL)
	{
		this->ClearLinearSystemPreconditioner();
		imode_ = WAITING_PICK;
	}
	if (this->is_cleared_ls_prec)
	{
		ls.AddPattern_Field(id_field_hinge, world);
		stitch_ary_.AddPatternLinearSystem(ls, world);
		for (unsigned int iiff = 0; iiff < aIdField_Fix.size(); iiff++)
		{
			const unsigned int id_field0 = aIdField_Fix[iiff];
			ls.SetFixedBoundaryCondition_Field(id_field0, world);
		}
		stitch_ary_.SetReorderingPreconditionerIfNeeded(prec, id_field_disp, world);
		prec.SetFillInLevel(0);
		prec.SetLinearSystem(ls.m_ls);
	}
	{
		Fem::Field::CField& field_lx = world.GetField(id_field_lamX);
		Fem::Field::CField& field_ly = world.GetField(id_field_lamY);
		Fem::Field::CNodeAry::CNodeSeg& ns_lx = field_lx.GetNodeSeg(CORNER, true, world, VALUE);
		Fem::Field::CNodeAry::CNodeSeg& ns_ly = field_ly.GetNodeSeg(CORNER, true, world, VALUE);
		const unsigned int nnode = ns_lx.Size();
		assert(ns_ly.Size() == nnode);
		assert(har.size() == nnode * 4);
		for (unsigned int ino = 0; ino < nnode; ino++)
		{
			ns_lx.SetValue(ino, 0, har[ino * 4 + 0]);
			ns_lx.SetValue(ino, 1, har[ino * 4 + 2]);
			ns_ly.SetValue(ino, 0, har[ino * 4 + 1]);
			ns_ly.SetValue(ino, 1, har[ino * 4 + 3]);
		}
	}
	if (imode_sensitivity_guess_ == 0)
	{
		imode_ = SENSITIVITY_DONE;
		return;
	}
	for (unsigned int iss = 0; iss < aSolSens.size(); iss++) { aSolSens[iss].is_active = false; }
	aSolSens[0].is_active = true;
	aSolSens[0].obj_x = obj_x;
	aSolSens[0].obj_y = obj_y;
	unsigned int id_field_senseX = aSolSens[0].id_field_dudpx;
	unsigned int id_field_senseY = aSolSens[0].id_field_dudpy;
	unsigned int id_field_x = aSolSens[0].id_field_x;
	SetDeformedValue(id_field_x, id_field_disp, world);
	//  GetSensitivity
	bool is_calc_sens = GetSensitivity_fictbend
		(ls, prec,
		////
		cloth_param,
		gx_, gy_, gz_,
		////
		contact_param,
		*pCT,
		aFrictionPoint,
		/////
		stitch_ary_,
		/////
		id_field_disp,
		id_field_hinge,
		////
		true,
		id_field_senseX, id_field_senseY,
		id_field_lamX, id_field_lamY,
		world);
	CopyValueVelo(id_field_disp_buffer, id_field_disp, world);
	if (is_calc_sens) { imode_ = SENSITIVITY_DONE; return; }
	imode_ = SIMULATION_STATIC;
}

/*
static double TriArea3D(const double v1[3], const double v2[3], const double v3[3]){
double x, y, z;
x = ( v2[1] - v1[1] )*( v3[2] - v1[2] ) - ( v3[1] - v1[1] )*( v2[2] - v1[2] );
y = ( v2[2] - v1[2] )*( v3[0] - v1[0] ) - ( v3[2] - v1[2] )*( v2[0] - v1[0] );
z = ( v2[0] - v1[0] )*( v3[1] - v1[1] ) - ( v3[0] - v1[0] )*( v2[1] - v1[1] );
return 0.5*sqrt( x*x + y*y + z*z );
}
*/


void CAnalysis2D_Cloth_Static::GetSensitivityElem_Slider
(const unsigned int no[3], const double r[3],
double org[3], double sns[3]) const
{
	if (!aSolSens[0].is_active) return;
	unsigned int id_field_senseX = aSolSens[0].id_field_dudpx;
	const Fem::Field::CField& field_sx = world.GetField(id_field_senseX);
	const Fem::Field::CNodeAry::CNodeSeg& ns_sx = field_sx.GetNodeSeg(CORNER, true, world, VALUE);

	if (no[0] >= ns_sx.Size()) return;
	if (no[1] >= ns_sx.Size()) return;
	if (no[2] >= ns_sx.Size()) return;

	const Fem::Field::CField& field_u = world.GetField(id_field_disp);
	const Fem::Field::CNodeAry::CNodeSeg& ns_C = field_u.GetNodeSeg(CORNER, false, world, VALUE);
	const Fem::Field::CNodeAry::CNodeSeg& ns_u = field_u.GetNodeSeg(CORNER, true, world, VALUE);

	org[0] = 0; org[1] = 0; org[2] = 0;
	sns[0] = 0; sns[1] = 0; sns[2] = 0;
	for (unsigned int i = 0; i < 3; i++)
	{
		unsigned int ino = no[i];
		double s[3];  ns_sx.GetValue(ino, s);
		double ratio = r[i];
		sns[0] += s[0] * ratio;
		sns[1] += s[1] * ratio;
		sns[2] += s[2] * ratio;
		double C[3];  ns_C.GetValue(ino, C);
		double u[3];  ns_u.GetValue(ino, u);
		org[0] += (C[0] + u[0])*ratio;
		org[1] += (C[1] + u[1])*ratio;
		org[2] += (C[2] + u[2])*ratio;
	}
}

bool CAnalysis2D_Cloth_Static::Pick(double scrx, double scry,
	const double trans0[], const double rot[], const double trans1[],
	unsigned int picked_elem_nodes[3], double picked_elem_ratio[3],
	unsigned int& id_l_cad)
{
	//  std::cout << "Put Pin" << scrx << " " << scry << std::endl;
	const Fem::Field::CField& field_disp = world.GetField(id_field_disp);
	const Fem::Field::CNodeAry::CNodeSeg& ns_c = field_disp.GetNodeSeg(CORNER, false, world, VALUE);
	const Fem::Field::CNodeAry::CNodeSeg& ns_u = field_disp.GetNodeSeg(CORNER, true, world, VALUE);
	std::vector<unsigned int> aIdEA = field_disp.GetAryIdEA();
	for (unsigned int iiea = 0; iiea < aIdEA.size(); iiea++)
	{
		const unsigned int id_ea = aIdEA[iiea];
		const Fem::Field::CElemAry::CElemSeg& es = field_disp.GetElemSeg(id_ea, CORNER, true, world);
		assert(es.Length() == 3);
		for (unsigned int ielem = 0; ielem < es.Size(); ielem++)
		{
			unsigned int no[3];	es.GetNodes(ielem, no);
			double C[3][3];	ns_c.GetValue(no[0], C[0]);	ns_c.GetValue(no[1], C[1]);	ns_c.GetValue(no[2], C[2]);
			double u[3][3];	ns_u.GetValue(no[0], u[0]);	ns_u.GetValue(no[1], u[1]);	ns_u.GetValue(no[2], u[2]);
			double c0[3][3] = {
				{ C[0][0] + u[0][0], C[0][1] + u[0][1], C[0][2] + u[0][2] },
				{ C[1][0] + u[1][0], C[1][1] + u[1][1], C[1][2] + u[1][2] },
				{ C[2][0] + u[2][0], C[2][1] + u[2][1], C[2][2] + u[2][2] } };
			double area0 = Com::TriArea3D(c0[0], c0[1], c0[2]);
			double c1[3][2];
			for (unsigned int i = 0; i < 3; i++)
			{
				const double c2[3] = { c0[i][0] + trans0[0], c0[i][1] + trans0[1], c0[i][2] + trans0[2] };
				const double c3[3] = {
					rot[0] * c2[0] + rot[1] * c2[1] + rot[2] * c2[2],
					rot[3] * c2[0] + rot[4] * c2[1] + rot[5] * c2[2],
					rot[6] * c2[0] + rot[7] * c2[1] + rot[8] * c2[2] };
				c1[i][0] = c3[0] + trans1[0];
				c1[i][1] = c3[1] + trans1[1];
			}
			double area1 = Com::TriArea2D(c1[0], c1[1], c1[2]);
			if (area1 < area0*0.01) continue;
			const double scr[2] = { scrx, scry };
			double area2 = Com::TriArea2D(scr, c1[1], c1[2]);
			double area3 = Com::TriArea2D(c1[0], scr, c1[2]);
			double area4 = Com::TriArea2D(c1[0], c1[1], scr);
			if (area2 < -area1*0.01) continue;
			if (area3 < -area1*0.01) continue;
			if (area4 < -area1*0.01) continue;
			std::cout << "hit " << id_ea << " " << ielem << std::endl;
			picked_elem_nodes[0] = no[0];
			picked_elem_nodes[1] = no[1];
			picked_elem_nodes[2] = no[2];
			picked_elem_ratio[0] = area2 / area1;
			picked_elem_ratio[1] = area3 / area1;
			picked_elem_ratio[2] = area4 / area1;
			const Fem::Field::CIDConvEAMshCad& conv = world.GetIDConverter(id_field_base);
			Cad::CAD_ELEM_TYPE itype;
			conv.GetIdCad_fromIdEA(id_ea, id_l_cad, itype);
			return true;
			/*
			for(unsigned int i=0;i<3;i++){
			if( aFrictionPoint[no[i]].is_pin ) continue;
			this->aFrictionPoint[no[i]].is_pin = true;
			this->aFrictionPoint[no[i]].aloc[0] = c0[i][0];
			this->aFrictionPoint[no[i]].aloc[1] = c0[i][1];
			this->aFrictionPoint[no[i]].aloc[2] = c0[i][2];
			}
			*/
		}
	}
	id_l_cad = 0;
	return false;
}

bool CAnalysis2D_Cloth_Static::Pick(ldp::Double2 screenPos, const ldp::Camera& cam,
	ldp::UInt3& picked_elem_nodes, ldp::Double3& picked_elem_ratio, unsigned int& id_l_cad,
	ldp::Double3& pickedScreenPos)
{
	//  std::cout << "Put Pin" << scrx << " " << scry << std::endl;
	const Fem::Field::CField& field_disp = world.GetField(id_field_disp);
	const Fem::Field::CNodeAry::CNodeSeg& ns_c = field_disp.GetNodeSeg(CORNER, false, world, VALUE);
	const Fem::Field::CNodeAry::CNodeSeg& ns_u = field_disp.GetNodeSeg(CORNER, true, world, VALUE);
	std::vector<unsigned int> aIdEA = field_disp.GetAryIdEA();
	id_l_cad = 0;
	for (unsigned int iiea = 0; iiea < aIdEA.size(); iiea++)
	{
		const unsigned int id_ea = aIdEA[iiea];
		const Fem::Field::CElemAry::CElemSeg& es = field_disp.GetElemSeg(id_ea, CORNER, true, world);
		assert(es.Length() == 3);
		for (unsigned int ielem = 0; ielem < es.Size(); ielem++)
		{
			unsigned int no[3];
			es.GetNodes(ielem, no);
			ldp::Double3 C[3], u[3], c0[3], C1[3];
			ldp::Double2 c1[3];
			for (int k = 0; k < 3; k++)
			{
				ns_c.GetValue(no[k], C[k].ptr());
				ns_u.GetValue(no[k], u[k].ptr());
				c0[k] = C[k] + u[k];
				C1[k] = cam.getScreenCoords(c0[k]);
				c1[k] = ldp::Float2(C1[k][0], C1[k][1]);
			}
			double area0 = Com::TriArea3D(c0[0].ptr(), c0[1].ptr(), c0[2].ptr());
			double area1 = Com::TriArea2D(c1[0].ptr(), c1[1].ptr(), c1[2].ptr());
			if (abs(area1) < area0*0.01) continue;
			const double scr[2] = { screenPos[0], screenPos[1] };
			double area2 = Com::TriArea2D(scr, c1[1].ptr(), c1[2].ptr());
			double area3 = Com::TriArea2D(c1[0].ptr(), scr, c1[2].ptr());
			double area4 = Com::TriArea2D(c1[0].ptr(), c1[1].ptr(), scr);
			if (std::signbit(area2) != std::signbit(area1)) continue;
			if (std::signbit(area3) != std::signbit(area1)) continue;
			if (std::signbit(area4) != std::signbit(area1)) continue;
			picked_elem_nodes[0] = no[0];
			picked_elem_nodes[1] = no[1];
			picked_elem_nodes[2] = no[2];
			picked_elem_ratio[0] = area2 / area1;
			picked_elem_ratio[1] = area3 / area1;
			picked_elem_ratio[2] = area4 / area1;
			ldp::Double3 tmpScreenPos = picked_elem_ratio[0] * C1[0] + picked_elem_ratio[1] * C1[1]
				+ picked_elem_ratio[2] * C1[2];
			// we only want the nearest..
			if (id_l_cad == 0 || (id_l_cad && tmpScreenPos[2] < pickedScreenPos[2]))
			{
				std::cout << "hit " << id_ea << " " << ielem << std::endl;
				pickedScreenPos = tmpScreenPos;
				const Fem::Field::CIDConvEAMshCad& conv = world.GetIDConverter(id_field_base);
				Cad::CAD_ELEM_TYPE itype;
				conv.GetIdCad_fromIdEA(id_ea, id_l_cad, itype);
			}
		} // end for ielem
	} // iiea
	return id_l_cad != 0;
}

void CAnalysis2D_Cloth_Static::SetIsDetail(bool is_detail, const Cad::CCadObj2D& cad_2d, const Msh::CMesher2D& mesh_2d)
{
	if (is_detail_ == is_detail) return;
	if (is_detail)
	{
		MakeDetailField(cad_2d, mesh_2d);
	}
	else
	{
		ClearDetailField();
		if (imode_ == SIMULATION_DETAIL)
		{
			imode_ = WAITING_PICK;
			this->ClearLinearSystemPreconditioner();
		}
	}
	is_detail_ = is_detail;
	InitDrawer();
}

void CAnalysis2D_Cloth_Static::MakeDetailField(const Cad::CCadObj2D& cad_2d, const Msh::CMesher2D& mesh_2d)
{
	Msh::CMesher2D mesh_2d_detail;
	{
		const Fem::Field::CIDConvEAMshCad& conv = world.GetIDConverter(id_field_base);
		Fem::Field::CField& field = world.GetField(id_field_base);
		const std::vector<unsigned int>& aIdEA = field.GetAryIdEA();
		for (unsigned int iiea = 0; iiea < aIdEA.size(); iiea++)
		{
			unsigned int id_ea = aIdEA[iiea];
			unsigned int id_l;
			Cad::CAD_ELEM_TYPE type;
			conv.GetIdCad_fromIdEA(id_ea, id_l, type);
			if (type != Cad::LOOP) continue;
			mesh_2d_detail.AddIdLCad_CutMesh(id_l);
		}
	}
	mesh_2d_detail.SetMeshingMode_ElemSize(m_num_detail_mesh_tri);
	mesh_2d_detail.Meshing(cad_2d);
	std::cout << " number of fine mesh node " << mesh_2d_detail.GetVectorAry().size() << std::endl;
	id_field_base_detail = world.AddMesh(Msh::CMeshProjector2Dto3D(mesh_2d_detail));
	id_field_disp_detail = world.MakeField_FieldElemDim(id_field_base_detail, 2, VECTOR3, VALUE | VELOCITY, CORNER);
	id_field_disp_buffer_detail = world.MakeField_FieldElemDim(id_field_base_detail, 2, VECTOR3, VALUE | VELOCITY, CORNER);
	id_field_hinge_detail = MakeHingeField_Tri(world, id_field_disp_detail);
	////
	FindBaseInterp(id_field_base_detail,
		id_field_base,
		world,
		aInterp_detail);
	std::cout << "stitch ary " << stitch_ary_.aStitch.size() << std::endl;
	stitch_ary_detail.CopyCADSituation(stitch_ary_);
	stitch_ary_detail.MakeField(id_field_disp_detail, id_field_base_detail, world);
	////
	{
		const Fem::Field::CField& field_disp_detail = world.GetField(id_field_disp_detail);
		const CNodeAry::CNodeSeg& ns_co = field_disp_detail.GetNodeSeg(CORNER, false, world, VALUE);
		const unsigned int nno = ns_co.Size();
		aFrictionPoint_detail.clear();
		aFrictionPoint_detail.resize(nno);
	}
	{
		aIdField_Fix_detail.clear();
		const Fem::Field::CIDConvEAMshCad& conv = world.GetIDConverter(id_field_base_detail);
		for (unsigned int iide = 0; iide < aIdECad_Fix.size(); iide++)
		{
			const unsigned int id_e = aIdECad_Fix[iide];
			aIdField_Fix_detail.push_back(world.GetPartialField(id_field_disp_detail, conv.GetIdEA_fromCad(id_e, Cad::EDGE)));
		}
	}
	MoveFineDeformedCoord(id_field_disp_detail, id_field_base_detail,
		id_field_disp, id_field_base,
		world,
		aInterp_detail);
}


void CAnalysis2D_Cloth_Static::ClearDetailField()
{
	{
		//    std::cout << "del " << id_field_base_detail << " " << id_field_disp_detail << " " << id_field_hinge_detail << std::endl;
		std::vector<unsigned int> aIdFieldDel;
		aIdFieldDel.push_back(id_field_base_detail);
		aIdFieldDel.push_back(id_field_disp_detail);
		aIdFieldDel.push_back(id_field_disp_buffer_detail);
		aIdFieldDel.push_back(id_field_hinge_detail);
		const std::vector<unsigned int>& aIdFieldStitch = this->stitch_ary_detail.GetIdFieldAry();
		for (unsigned int ist = 0; ist < aIdFieldStitch.size(); ist++)
		{
			aIdFieldDel.push_back(aIdFieldStitch[ist]);
		}
		world.DeleteField(aIdFieldDel);
	}
	id_field_base_detail = 0;
	id_field_disp_detail = 0;
	id_field_hinge_detail = 0;
	stitch_ary_detail.Clear();
	this->aFrictionPoint_detail.clear();
	this->aInterp_detail.clear();
	this->ClearLinearSystemPreconditioner();
	is_detail_ = false;
}

void CAnalysis2D_Cloth_Static::SaveTimeStamp(const Cad::CCadObj2D& cad_2d, const Msh::CMesher2D& mesh_2d)
{
	this->time_stamp.cad = cad_2d;
	this->time_stamp.msh = mesh_2d;
	this->time_stamp.cloth_param = this->cloth_param;
	this->time_stamp.contact_param = this->contact_param;
	this->time_stamp.dt = this->dt_;
	this->time_stamp.cur_time = this->cur_time_;
	this->time_stamp.world = this->world;

	this->time_stamp.id_field_base_ = id_field_base;
	this->time_stamp.id_field_disp_ = id_field_disp;
	this->time_stamp.id_field_disp_buffer_ = id_field_disp_buffer;
	this->time_stamp.id_field_hinge_ = id_field_hinge;
	this->time_stamp.stitch_ary_ = stitch_ary_;
	this->time_stamp.aFrictionPoint = aFrictionPoint;

	// detail
	this->time_stamp.is_detail = is_detail_;
	this->time_stamp.aInterp_detail = aInterp_detail;
	this->time_stamp.id_field_base_detail_ = id_field_base_detail;
	this->time_stamp.id_field_disp_detail_ = id_field_disp_detail;
	this->time_stamp.id_field_disp_buffer_detail = id_field_disp_buffer_detail;
	this->time_stamp.id_field_hinge_detail = id_field_hinge_detail;
	this->time_stamp.stitch_ary_detail = stitch_ary_detail;
	this->time_stamp.aFrictionPoint_detail = aFrictionPoint_detail;

	// sensitivity analysis
	this->time_stamp.aSolSens = aSolSens;
	this->time_stamp.id_field_lamX = id_field_lamX;
	this->time_stamp.id_field_lamY = id_field_lamY;
}

void CAnalysis2D_Cloth_Static::LoadTimeStamp(Cad::CCadObj2D& cad_2d, Msh::CMesher2D& mesh_2d)
{
	cad_2d = this->time_stamp.cad;
	mesh_2d = this->time_stamp.msh;
	this->cloth_param = this->time_stamp.cloth_param;
	this->contact_param = this->time_stamp.contact_param;
	this->cur_time_ = this->time_stamp.cur_time;
	this->dt_ = this->time_stamp.dt;
	this->world = time_stamp.world;

	this->id_field_base = time_stamp.id_field_base_;
	this->id_field_disp = time_stamp.id_field_disp_;
	this->id_field_disp_buffer = time_stamp.id_field_disp_buffer_;
	this->id_field_hinge = time_stamp.id_field_hinge_;
	this->stitch_ary_ = time_stamp.stitch_ary_;
	this->aFrictionPoint = time_stamp.aFrictionPoint;

	// detail
	this->is_detail_ = time_stamp.is_detail;
	this->aInterp_detail = time_stamp.aInterp_detail;
	this->id_field_base_detail = time_stamp.id_field_base_detail_;
	this->id_field_disp_detail = time_stamp.id_field_disp_detail_;
	this->id_field_disp_buffer_detail = time_stamp.id_field_disp_buffer_detail;
	this->id_field_hinge_detail = time_stamp.id_field_hinge_detail;
	this->stitch_ary_detail = time_stamp.stitch_ary_detail;
	this->aFrictionPoint_detail = time_stamp.aFrictionPoint_detail;

	// sensitivity analysis
	this->aSolSens = time_stamp.aSolSens;
	this->id_field_lamX = time_stamp.id_field_lamX;
	this->id_field_lamY = time_stamp.id_field_lamY;

	////
	clothHandler_.BuildClothMeshTopology(id_field_base, id_field_disp, world);
	this->ClearLinearSystemPreconditioner();
	this->InitDrawer();
	this->PerformStaticSolver();
}

void CAnalysis2D_Cloth_Static::MeshQualityInfo(double& max_aspect, bool& is_inverted) const
{
	// fine
	if (!world.IsIdField(id_field_base_detail))
	{
		max_aspect = 1.0;
		is_inverted = false;
		return;
	}
	const Fem::Field::CField& field0 = world.GetField(id_field_disp_detail);
	const Fem::Field::CNodeAry::CNodeSeg& ns_c0 = field0.GetNodeSeg(CORNER, false, world, VALUE);
	const std::vector<unsigned int> aIdEA0 = field0.GetAryIdEA();
	is_inverted = false;
	max_aspect = -1;
	for (unsigned int iiea0 = 0; iiea0 < aIdEA0.size(); iiea0++)
	{
		const unsigned int id_ea0 = aIdEA0[iiea0];
		const Fem::Field::CElemAry::CElemSeg& es0 = field0.GetElemSeg(id_ea0, CORNER, false, world);
		assert(es0.Length() == 3);
		for (unsigned int ielem0 = 0; ielem0 < es0.Size(); ielem0++)
		{
			unsigned int no0[3];	es0.GetNodes(ielem0, no0);
			double C0[3][3];
			for (unsigned int inoes0 = 0; inoes0<3; inoes0++)
			{
				unsigned int ino0 = no0[inoes0];
				ns_c0.GetValue(ino0, C0[inoes0]);
			}
			const double area = fabs(Com::TriArea2D(C0[0], C0[1], C0[2]));
			const double len0 = Com::Distance2D(C0[1], C0[2]);
			const double len1 = Com::Distance2D(C0[0], C0[2]);
			const double len2 = Com::Distance2D(C0[0], C0[1]);
			double max_len = len0;
			if (len1 > max_len) max_len = len1;
			if (len2 > max_len) max_len = len2;
			double aspect = max_len * (len0 + len1 + len2) * 0.5 / area;
			if (aspect > max_aspect) max_aspect = aspect;
			if (area < 0)
			{
				is_inverted = true;
				break;
			}
		}
	}


}

static unsigned int MakeHingeField_Tri(Fem::Field::CFieldWorld& world, unsigned int id_field_base)
{
	assert(world.IsIdField(id_field_base));
	const CField& field_base = world.GetField(id_field_base);

	std::vector<Fem::Field::CField::CElemInterpolation> aElemIntp;
	unsigned int id_na = field_base.GetNodeSegInNodeAry(CORNER).id_na_va;
	for (unsigned int iiea = 0; iiea < field_base.GetAryIdEA().size(); iiea++)
	{
		unsigned int id_ea0 = field_base.GetAryIdEA()[iiea];
		unsigned int id_es_co = field_base.GetIdElemSeg(id_ea0, CORNER, false, world);
		const CElemAry& ea = world.GetEA(id_ea0);
		int* elsuel = new int[ea.Size() * 3];
		ea.MakeElemSurElem(id_es_co, elsuel);

		unsigned int nedge = 0;
		{
			const CElemAry::CElemSeg& es = ea.GetSeg(id_es_co);
			for (unsigned int ielem = 0; ielem < ea.Size(); ielem++)
			{
				unsigned int no[3];
				es.GetNodes(ielem, no);
				if (elsuel[ielem * 3 + 0] >= 0 && no[1] < no[2]) { nedge++; }
				if (elsuel[ielem * 3 + 1] >= 0 && no[2] < no[0]) { nedge++; }
				if (elsuel[ielem * 3 + 2] >= 0 && no[0] < no[1]) { nedge++; }
			}
		}

		std::vector<int> lnods;
		{
			lnods.resize(nedge * 4);
			unsigned int iedge = 0;
			const CElemAry::CElemSeg& es = ea.GetSeg(id_es_co);
			for (unsigned int ielem = 0; ielem < ea.Size(); ielem++)
			{
				unsigned int no[3];	es.GetNodes(ielem, no);
				if (elsuel[ielem * 3 + 0] >= 0 && no[1] < no[2])
				{
					lnods[iedge * 4 + 0] = no[0];	lnods[iedge * 4 + 2] = no[1];	lnods[iedge * 4 + 3] = no[2];
					const unsigned int ielem0 = elsuel[ielem * 3 + 0];
					unsigned int no0[3];	es.GetNodes(ielem0, no0);
					if (no0[1] == no[2] && no0[2] == no[1]) { lnods[iedge * 4 + 1] = no0[0]; }
					else if (no0[2] == no[2] && no0[0] == no[1]) { lnods[iedge * 4 + 1] = no0[1]; }
					else if (no0[0] == no[2] && no0[1] == no[1]) { lnods[iedge * 4 + 1] = no0[2]; }
					else { assert(0); }
					iedge++;
				}
				if (elsuel[ielem * 3 + 1] >= 0 && no[2] < no[0])
				{
					lnods[iedge * 4 + 0] = no[1];	lnods[iedge * 4 + 2] = no[2];	lnods[iedge * 4 + 3] = no[0];
					const unsigned int ielem0 = elsuel[ielem * 3 + 1];
					unsigned int no0[3];	es.GetNodes(ielem0, no0);
					if (no0[1] == no[0] && no0[2] == no[2]) { lnods[iedge * 4 + 1] = no0[0]; }
					else if (no0[2] == no[0] && no0[0] == no[2]) { lnods[iedge * 4 + 1] = no0[1]; }
					else if (no0[0] == no[0] && no0[1] == no[2]) { lnods[iedge * 4 + 1] = no0[2]; }
					else { assert(0); }
					iedge++;
				}
				if (elsuel[ielem * 3 + 2] >= 0 && no[0] < no[1])
				{
					lnods[iedge * 4 + 0] = no[2];	lnods[iedge * 4 + 2] = no[0];	lnods[iedge * 4 + 3] = no[1];
					const unsigned int ielem0 = elsuel[ielem * 3 + 2];
					unsigned int no0[3];	es.GetNodes(ielem0, no0);
					if (no0[1] == no[1] && no0[2] == no[0]) { lnods[iedge * 4 + 1] = no0[0]; }
					else if (no0[2] == no[1] && no0[0] == no[0]) { lnods[iedge * 4 + 1] = no0[1]; }
					else if (no0[0] == no[1] && no0[1] == no[0]) { lnods[iedge * 4 + 1] = no0[2]; }
					else { assert(0); }
					iedge++;
				}
			}
			assert(iedge == nedge);
		}

		unsigned int id_ea_add = world.AddElemAry(nedge, QUAD);
		unsigned int id_es_add = 0;
		{
			CElemAry& ea_add = world.GetEA(id_ea_add);
			id_es_add = ea_add.AddSegment(0, CElemAry::CElemSeg(id_na, CORNER), lnods);
		}

		delete[] elsuel;

		Fem::Field::CField::CElemInterpolation ei(id_ea_add, id_es_add, id_es_add, 0, 0, 0, 0);
		aElemIntp.push_back(ei);
	}

	const Fem::Field::CField::CNodeSegInNodeAry na_c = field_base.GetNodeSegInNodeAry(CORNER);
	const Fem::Field::CField::CNodeSegInNodeAry na_b;

	unsigned int id_field = world.AddField(id_field_base, aElemIntp, na_c, na_b);
	assert(world.IsIdField(id_field));
	return id_field;
}

////////////////////////////////////////////////////////////ldp////////////////////////////////////////////////////

static void makeCurveFromSvgCurve(Cad::CCadObj2D_Move& cad_2d, const std::vector<float>& pathCoords, 
	int eid, float pixel2meter)
{
	const auto& coords = pathCoords;
	std::vector<Com::CVector2D> pc;
	if (coords.size() <= 4)
		return; // only two points, must be a straight line
	ldp::Float2 lastp(coords[0], coords[1]); // we should avoid overlapped points
	for (size_t i = 2; i < coords.size() - 2; i += 2)
	{
		ldp::Float2 p(coords[i], coords[i + 1]);
		if ((p - lastp).length() < std::numeric_limits<float>::epsilon())
			continue;
		pc.push_back(Com::CVector2D(p[0], p[1])*pixel2meter);
		lastp = p;
	}
	if (!cad_2d.SetCurve_Polyline(eid, pc))
		printf("error: set curve failed: %d!\n", eid);
}

static void makeCurveFromSvgCurve(Cad::CCadObj2D_Move& cad_2d, const svg::SvgPolyPath* path,
	const Cad::CCadObj2D::CResAddPolygon& loop, int iEdge, float pixel2meter)
{
	const auto& coords = path->getEdgeCoords(iEdge);
	std::vector<Com::CVector2D> pc;
	if (coords.size() <= 4)
		return; // only two points, must be a straight line
	ldp::Float2 lastp(coords[0], coords[1]); // we should avoid overlapped points
	for (size_t i = 2; i < coords.size() - 2; i += 2)
	{
		ldp::Float2 p(coords[i], coords[i + 1]);
		if ((p - lastp).length() < std::numeric_limits<float>::epsilon())
			continue;
		pc.push_back(Com::CVector2D(p[0], p[1])*pixel2meter);
		lastp = p;
	}
	if (!cad_2d.SetCurve_Polyline(loop.aIdE[iEdge], pc))
		printf("error: set curve failed: %d %d!\n", path->getId(), iEdge);
}

struct EdgeWithPleats
{
	int vert_id_s;
	int vert_id_e;
	std::set<unsigned int> pleatsEdgeIds;
	std::set<unsigned int> splittedEdgeIds;
	EdgeWithPleats() : vert_id_s(0), vert_id_e(0) {}

	// filled by makeOrdered
	std::vector<unsigned int> splittedEdgeIdsOrdered;
	std::vector<float> splittedEdgeLengths;
	std::vector<bool> splittedEdgeValid;
	std::map<unsigned int, int> splittedEdgeIdToArrayPosMap;
	std::map<unsigned int, std::set<unsigned int>> splittedVertsToEdgeMap;
	std::map<unsigned int, std::set<unsigned int>> edgesWithContactPleats;

	void makeOrdered(const Cad::CCadObj2D_Move& cad)
	{
		splittedVertsToEdgeMap.clear();
		// counting
		for (auto eid : splittedEdgeIds)
		{
			auto edge = cad.GetEdge(eid);
			std::set<unsigned int> eids;
			eids.insert(eid);

			auto eiter = splittedVertsToEdgeMap.find(edge.id_v_s);
			if (eiter == splittedVertsToEdgeMap.end())
				splittedVertsToEdgeMap.insert(std::make_pair(edge.id_v_s, eids));
			else
				eiter->second.insert(eid);

			eiter = splittedVertsToEdgeMap.find(edge.id_v_e);
			if (eiter == splittedVertsToEdgeMap.end())
				splittedVertsToEdgeMap.insert(std::make_pair(edge.id_v_e, eids));
			else
				eiter->second.insert(eid);
		} // end for auto id : pleatsEdgeIds

		// find the minimal-cnt edge
		int cnt1num = 0;
		for (auto iter : splittedVertsToEdgeMap)
		{
			int cnt = iter.second.size();
			int vid = iter.first;
			if (cnt >= 3)
				throw std::exception("non-single-dir edges found!");
			if (cnt == 1)
				cnt1num++;
		} // end for auto iter : pleatVertsCnt
		if (cnt1num > 2)
			throw std::exception("not a connected edge");
		auto vbegin = splittedVertsToEdgeMap.find(vert_id_s);
		if (vbegin == splittedVertsToEdgeMap.end())
			throw std::exception("construction error");
		if (vbegin->second.size() != 1)
			throw std::exception("construction error1");

		// start from the minimal-cnt edge and go through
		splittedEdgeIdsOrdered.clear();
		splittedEdgeLengths.clear();
		splittedEdgeValid.clear();
		splittedEdgeIdToArrayPosMap.clear();
		int vid = vert_id_s;
		std::set<unsigned int> eidVisited;
		while (1)
		{
			const auto& eids = splittedVertsToEdgeMap[vid];
			int nvid = 0;
			for (auto eid : eids)
			{
				if (eidVisited.find(eid) != eidVisited.end())
					continue;
				auto edge = cad.GetEdge(eid);
				nvid = edge.id_v_s;
				if (nvid == vid) nvid = edge.id_v_e;
				eidVisited.insert(eid);
				splittedEdgeIdToArrayPosMap.insert(std::make_pair(eid, (int)splittedEdgeIdsOrdered.size()));
				splittedEdgeIdsOrdered.push_back(eid);
				splittedEdgeLengths.push_back(edge.GetCurveLength());
				splittedEdgeValid.push_back(true);
				break;
			} // end for eid
			vid = nvid;
			if (vid == 0 || vid == vert_id_s)
				break;
		} // end while vid

		// build edge-pleats connections
		edgesWithContactPleats.clear();
		for (auto eid : pleatsEdgeIds)
		{
			auto edge = cad.GetEdge(eid);
			const auto& siter = splittedVertsToEdgeMap.find(edge.id_v_s);
			if (siter == splittedVertsToEdgeMap.end())
				continue;
			for (auto neid : siter->second)
			{
				auto& piter = edgesWithContactPleats.find(neid);
				if (piter == edgesWithContactPleats.end())
				{
					std::set<unsigned int> eids;
					edgesWithContactPleats.insert(std::make_pair(neid, eids));
					piter = edgesWithContactPleats.find(neid);
				}
				piter->second.insert(eid);
			}
		} // end for pleatsEdgeIds
	}

	void addPleatPair(Cad::CCadObj2D_Move& cad, int pleat1id, int pleat2id)
	{
		if (pleatsEdgeIds.find(pleat1id) == pleatsEdgeIds.end()
			|| pleatsEdgeIds.find(pleat2id) == pleatsEdgeIds.end())
			return;

		for (const auto& piter : edgesWithContactPleats)
		{
			const auto& pleatIds = piter.second;
			if (pleatIds.size() != 2)
				continue;
			if (pleatIds.find(pleat1id) != pleatIds.end() && pleatIds.find(pleat2id) != pleatIds.end())
				splittedEdgeValid[splittedEdgeIdToArrayPosMap[piter.first]] = false;
		}
	}

	static bool isSameDir(Cad::CCadObj2D_Move& cad_2d, Msh::CMesher2D& mesh, int id_e1, int id_e2)
	{
		

		bool is_left1 = true;
		{
			unsigned int id_l1_l, id_l1_r;
			cad_2d.GetIdLoop_Edge(id_l1_l, id_l1_r, id_e1);
			//assert( id_l1_l*id_l1_r == 0 );    // ldp comment this to enable pleats
			if (id_l1_l == 0 && id_l1_r != 0) { is_left1 = false; }
			if (id_l1_l != 0 && id_l1_r == 0) { is_left1 = true; }
		}
		bool is_left2 = false;
		{
			unsigned int id_l2_l, id_l2_r;
			cad_2d.GetIdLoop_Edge(id_l2_l, id_l2_r, id_e2);
			//assert( id_l2_l*id_l2_r == 0 );    // ldp comment this to enable pleats
			if (id_l2_l == 0 && id_l2_r != 0) { is_left2 = false; }
			if (id_l2_l != 0 && id_l2_r == 0) { is_left2 = true; }
		}
		bool dir = (is_left1 != is_left2);
		return dir;
	}

	void divideAnotherEdgeByThis(Cad::CCadObj2D_Move& cad, Msh::CMesher2D& mesh,
		int this_edge_id, int other_edge_id, EdgeWithPleats& newEdgeIdOrdered, bool& sameDir)const
	{
		sameDir = isSameDir(cad, mesh, this_edge_id, other_edge_id);
		std::vector<float> sumLens;
		sumLens.push_back(0);
		for (size_t i = 0; i < splittedEdgeLengths.size(); i++)
		{
			if (!splittedEdgeValid[i]) continue;
			sumLens.push_back(sumLens.back() + splittedEdgeLengths[i]);
		}
		if (sumLens.back() < std::numeric_limits<float>::epsilon())
			return;
		newEdgeIdOrdered.splittedEdgeIds.insert(other_edge_id);
		auto edge = cad.GetEdge(other_edge_id);
		newEdgeIdOrdered.vert_id_s = edge.id_v_s;
		newEdgeIdOrdered.vert_id_e = edge.id_v_e;
		auto po_s = edge.po_s;
		auto po_e = edge.po_e;
		if (!sameDir) std::swap(po_s, po_e);
		for (size_t i = 1; i < sumLens.size() - 1; i++)
		{
			float s = sumLens[i] / sumLens.back();
			Com::CVector2D p = po_s * (1 - s) + po_e * s;
			auto res = cad.AddVertex(Cad::EDGE, other_edge_id, p);
			if (res.id_e_add == 0)
			{
				for (auto eid : newEdgeIdOrdered.splittedEdgeIds)
				{
					res = cad.AddVertex(Cad::EDGE, eid, p);
					if (res.id_e_add)
						break;
				}
			}
			assert(res.id_e_add);
			newEdgeIdOrdered.splittedEdgeIds.insert(res.id_e_add);
		}
		newEdgeIdOrdered.makeOrdered(cad);
		if (!sameDir)
		{
			auto tmp = newEdgeIdOrdered.splittedEdgeIdsOrdered;
			std::copy(tmp.begin(), tmp.end(), newEdgeIdOrdered.splittedEdgeIdsOrdered.rbegin());
			auto tmp1 = newEdgeIdOrdered.splittedEdgeLengths;
			std::copy(tmp1.begin(), tmp1.end(), newEdgeIdOrdered.splittedEdgeLengths.rbegin());
			auto tmp2 = newEdgeIdOrdered.splittedEdgeValid;
			std::copy(tmp2.begin(), tmp2.end(), newEdgeIdOrdered.splittedEdgeValid.rbegin());
		}
	}
};

void CAnalysis2D_Cloth_Static::SetModelClothFromSvg(Cad::CCadObj2D_Move& cad_2d, Msh::CMesher2D& mesh_2d,
	svg::SvgManager* svgManager, CSliderDeform& slider_deform,
	std::vector< std::pair<unsigned int, unsigned int> >& aSymIdVPair,
	std::map<unsigned int, int>& loopId2svgIdMap,
	int num_coarse_mesh_tri, int num_detail_mesh_tri)
{
	// clear---------------------------------------------------------------------
	cad_2d.Clear();
	mesh_2d.Clear();
	stitch_ary_.Clear();
	aIdECad_Fix.clear();
	slider_deform.Clear();
	aSymIdVPair.clear();
	loopId2svgIdMap.clear();
	m_selfStichLoops.clear();

	m_num_coarse_mesh_tri = num_coarse_mesh_tri;
	m_num_detail_mesh_tri = num_detail_mesh_tri;

	// 1. create 2D pieces from svg ------------------------------------------------------------
	auto polyPaths = svgManager->collectPolyPaths(true);
	auto edgeGroups = svgManager->collectEdgeGroups(true);
	const float pixel2meter = svgManager->getPixelToMeters();
	if (polyPaths.size() == 0)
		throw std::exception("no selected polypaths given!");
	if (pixel2meter == 1.f)
		printf("warning: are you sure 1 pixel = 1 meter? \npossibly you should select \
			   			   the standard-4cm rectangle and click the \"pixel to meter\" button\n");
	std::vector<Cad::CCadObj2D::CResAddPolygon> polyLoops;
	std::vector<ldp::Float2> polyCenters;
	std::vector<ldp::Float3> poly3dCenters;
	std::vector<ldp::QuaternionF> poly3dRots;
	std::map<int, Cad::CCadObj2D::CResAddPolygon*> svg2loopMap;

	// 1.1 add closed polygons as loops ------------------------------------------------------
	polyLoops.reserve(polyPaths.size());
	for (auto polyPath : polyPaths)
	{
		polyCenters.push_back(polyPath->getCenter());
		poly3dCenters.push_back(polyPath->get3dCenter());
		poly3dRots.push_back(polyPath->get3dRot());
		// if non-closed, we ignore it this time
		if (!polyPath->isClosed())
		{
			polyLoops.push_back(Cad::CCadObj2D::CResAddPolygon());
			continue;
		}
		// if closed, we create a loop for it: 
		// firstly, straght lines added
		std::vector<Com::CVector2D> vec_ary;
		for (size_t i = 0; i < polyPath->numCorners(); i++)
		{
			ldp::Float2 p = polyPath->getCorner(i) * pixel2meter;
			vec_ary.push_back(Com::CVector2D(p[0], p[1]));
		}

		// this is a hack of Li-Bao-Jian's code: we are adding curved polygons,
		// firstly we add an straight polygon, we do not want intersection test here
		// then we make the polygon curved, and we recover the interection test
		cad_2d.ldpSetEdgeLoopIntersectionDisabled(true);
		polyLoops.push_back(cad_2d.AddPolygon(vec_ary));
		mesh_2d.AddIdLCad_CutMesh(polyLoops.back().id_l_add);
		loopId2svgIdMap.insert(std::make_pair(polyLoops.back().id_l_add, polyPath->getId()));
		svg2loopMap.insert(std::make_pair(polyPath->getId(), &polyLoops.back()));

		// then make curve
		for (size_t iEdge = 0; iEdge < polyLoops.back().aIdE.size(); iEdge++)
			makeCurveFromSvgCurve(cad_2d, polyPath, polyLoops.back(), iEdge, pixel2meter);
		cad_2d.ldpSetEdgeLoopIntersectionDisabled(false);
	} // end for polyPath

	// 1.2 add non-closed polygons as pleats -------------------------------------------------------
	std::map<unsigned int, EdgeWithPleats> edgesWithPleats;
	std::map<unsigned int, unsigned int> newEdgesToRootSplitEdgesMap, cornerDartPair;
	std::map<unsigned int, int> pleatsToPathIdMap, pathIdToPleatsMap;
	for (size_t iPoly = 0; iPoly < polyPaths.size(); iPoly++)
	{
		const auto& path_i = polyPaths[iPoly];
		const auto& loop_i = polyLoops[iPoly];
		if (loop_i.id_l_add)
			continue; // closed loops that we have handled
		std::vector<Com::CVector2D> pts;
		for (size_t i = 0; i < path_i->numCorners(); i++)
			pts.push_back(Com::CVector2D(path_i->getCorner(i)[0], path_i->getCorner(i)[1])*pixel2meter);
		if (pts.size() == 1)
			continue;
		const float on_segment_thre = (pts[0] - pts[1]).Length() * 0.2f;
		unsigned int foundLoopId = 0;

		// add pleat firstly
		if (pts.size() == 2)
		{
			int splittedEdgeId = 0, mergedVertId = 0;
			int newEdgeIds[2] = { 0 };
			cad_2d.addPleat_HemLine(pts[0], pts[1], on_segment_thre, foundLoopId,
				mergedVertId, splittedEdgeId, newEdgeIds);
			if (foundLoopId > 0)
			{
				if (splittedEdgeId)
				{
					int rootEdgeId = splittedEdgeId;
					const auto& rootIter = newEdgesToRootSplitEdgesMap.find(rootEdgeId);
					if (rootIter != newEdgesToRootSplitEdgesMap.end())
						rootEdgeId = rootIter->second;
					const auto& rootEdge = cad_2d.GetEdge(rootEdgeId);
					auto& iter = edgesWithPleats.find(rootEdgeId);
					if (iter == edgesWithPleats.end())
					{
						edgesWithPleats.insert(std::make_pair((unsigned int)rootEdgeId, EdgeWithPleats()));
						iter = edgesWithPleats.find(rootEdgeId);
						iter->second.vert_id_s = rootEdge.id_v_s;
						iter->second.vert_id_e = rootEdge.id_v_e;
					}
					newEdgesToRootSplitEdgesMap.insert(std::make_pair(splittedEdgeId, rootEdgeId));
					newEdgesToRootSplitEdgesMap.insert(std::make_pair(newEdgeIds[0], rootEdgeId));
					newEdgesToRootSplitEdgesMap.insert(std::make_pair(newEdgeIds[1], rootEdgeId));
					iter->second.splittedEdgeIds.insert(splittedEdgeId);
					iter->second.splittedEdgeIds.insert(newEdgeIds[0]);
					iter->second.pleatsEdgeIds.insert(newEdgeIds[1]);
					pleatsToPathIdMap.insert(std::make_pair(newEdgeIds[1], path_i->getId()));
					pathIdToPleatsMap.insert(std::make_pair(path_i->getId(), newEdgeIds[1]));
					printf("add pleat %d to loop %d\n", path_i->getId(), foundLoopId);
				} // end if edge id
				else if (mergedVertId)
				{
					pleatsToPathIdMap.insert(std::make_pair(newEdgeIds[0], path_i->getId()));
					pathIdToPleatsMap.insert(std::make_pair(path_i->getId(), newEdgeIds[0]));
					printf("add pleat %d to loop %d\n", path_i->getId(), foundLoopId);
				} // end if vert id
				else
				{
					makeCurveFromSvgCurve(cad_2d, path_i->getEdgeCoords(0), newEdgeIds[0], pixel2meter);
					printf("add hemline %d to loop %d\n", path_i->getId(), foundLoopId);
				}
			} // end if foundLoopId > 0
		} // end if pts.size() == 2
		else if (pts.size() == 3) // this means a corner dart
		{
			int splittedEdgeId = 0;
			int newEdgeIds[4] = { 0 };
			cad_2d.addCornerDart(pts.data(), on_segment_thre, foundLoopId, splittedEdgeId, newEdgeIds);
			if (splittedEdgeId)
			{
				int rootEdgeId = splittedEdgeId;
				const auto& rootIter = newEdgesToRootSplitEdgesMap.find(rootEdgeId);
				if (rootIter != newEdgesToRootSplitEdgesMap.end())
					rootEdgeId = rootIter->second;
				const auto& rootEdge = cad_2d.GetEdge(rootEdgeId);
				auto& iter = edgesWithPleats.find(rootEdgeId);
				if (iter == edgesWithPleats.end())
				{
					edgesWithPleats.insert(std::make_pair((unsigned int)rootEdgeId, EdgeWithPleats()));
					iter = edgesWithPleats.find(rootEdgeId);
					iter->second.vert_id_s = rootEdge.id_v_s;
					iter->second.vert_id_e = rootEdge.id_v_e;
				}
				newEdgesToRootSplitEdgesMap.insert(std::make_pair(splittedEdgeId, rootEdgeId));
				newEdgesToRootSplitEdgesMap.insert(std::make_pair(newEdgeIds[0], rootEdgeId));
				newEdgesToRootSplitEdgesMap.insert(std::make_pair(newEdgeIds[1], rootEdgeId));
				iter->second.splittedEdgeIds.insert(splittedEdgeId);
				iter->second.splittedEdgeIds.insert(newEdgeIds[0]);
				iter->second.splittedEdgeIds.insert(newEdgeIds[1]);
				iter->second.pleatsEdgeIds.insert(newEdgeIds[2]);
				iter->second.pleatsEdgeIds.insert(newEdgeIds[3]);
				cornerDartPair.insert(std::make_pair(newEdgeIds[2], newEdgeIds[3]));
				printf("add corner dart %d to loop %d\n", path_i->getId(), foundLoopId);
			} // end if edge id
		} // end if pts.size() == 3
	} // end for iPoly

	// 1.2.1 make the added pleats edges ordered and compute the lenths
	for (auto& iter : edgesWithPleats)
		iter.second.makeOrdered(cad_2d);

	// 1.3 make stitchings -------------------------------------------------------------------------
	std::vector< std::pair<unsigned int, unsigned int> > loopEdgePairsBeforePleats;
	std::vector< std::tuple<unsigned int, unsigned int, bool > > aIdECad_Stitch;
	// collect directly usable pairs and thoses borken by pleats
	for (const auto& eg : edgeGroups)
	{
		for (const auto& g1 : eg->group)
		{
			auto iter1 = svg2loopMap.find(g1.first->getId());
			if (iter1 == svg2loopMap.end()) continue;
			auto loop1 = iter1->second;
			for (const auto& g2 : eg->group)
			{
				const auto& iter2 = svg2loopMap.find(g2.first->getId());
				if (iter2 == svg2loopMap.end()) continue;
				const auto& loop2 = iter2->second;
				if (loop1->id_l_add < loop2->id_l_add) continue;
				if (loop1->id_l_add == loop2->id_l_add && g1.second <= g2.second) continue;
				unsigned int edge1 = loop1->aIdE[g1.second];
				unsigned int edge2 = loop2->aIdE[g2.second];
				const auto& edge1_s_iter = edgesWithPleats.find(edge1);
				const auto& edge2_s_iter = edgesWithPleats.find(edge2);
				if (edge1_s_iter == edgesWithPleats.end() && edge2_s_iter == edgesWithPleats.end())
				{
					bool sameDir = EdgeWithPleats::isSameDir(cad_2d, mesh_2d, edge1, edge2);
					aIdECad_Stitch.push_back(std::make_tuple(edge1, edge2, sameDir));
					// the two edges are stiched inside the same loop, this possibly means a cylinder initialization
					if (loop1->id_l_add == loop2->id_l_add)
					{
						auto& iter = m_selfStichLoops.find(loop1->id_l_add);
						if (iter == m_selfStichLoops.end())
						{
							m_selfStichLoops.insert(std::make_pair(loop1->id_l_add, SelfStichLoop()));
							iter = m_selfStichLoops.find(loop1->id_l_add);
						}
						iter->second.selfStiches.push_back(aIdECad_Stitch.back());
					}
				}
				else
					loopEdgePairsBeforePleats.push_back(std::make_pair(edge1, edge2));
			} // end for g2
		} // end for g1
	} // end for eg
	// collect pleats pairs
	for (const auto& eg : edgeGroups)
	{
		for (const auto& g1 : eg->group)
		{
			const auto& iter1 = pathIdToPleatsMap.find(g1.first->getId());
			if (iter1 == pathIdToPleatsMap.end()) continue;
			for (const auto& g2 : eg->group)
			{
				const auto& iter2 = pathIdToPleatsMap.find(g2.first->getId());
				if (iter2 == pathIdToPleatsMap.end()) continue;
				if (iter1->second <= iter2->second) continue;
				bool sameDir = EdgeWithPleats::isSameDir(cad_2d, mesh_2d, iter1->second, iter2->second);
				aIdECad_Stitch.push_back(std::make_tuple(iter1->second, iter2->second, sameDir));
				for (auto& epiter : edgesWithPleats)
					epiter.second.addPleatPair(cad_2d, iter1->second, iter2->second);
			} // end for g2
		} // end for g1
	} // end for eg
	// collect corner dart pairs
	for (const auto& pair : cornerDartPair)
	{
		bool sameDir = EdgeWithPleats::isSameDir(cad_2d, mesh_2d, pair.second, pair.second);
		aIdECad_Stitch.push_back(std::make_tuple(pair.first, pair.second, sameDir));
		for (auto& epiter : edgesWithPleats)
			epiter.second.addPleatPair(cad_2d, pair.first, pair.second);
	}
	// collect edge pairs broken by pleats
	for (const auto& pair_iter : loopEdgePairsBeforePleats)
	{
		unsigned int edge1 = pair_iter.first;
		unsigned int edge2 = pair_iter.second;
		auto edge1_s_iter = edgesWithPleats.find(pair_iter.first);
		auto edge2_s_iter = edgesWithPleats.find(pair_iter.second);
		unsigned int loop1, loop2, loop_tmpl, loop_tmpr;
		cad_2d.GetIdLoop_Edge(loop_tmpl, loop_tmpr, edge1);
		loop1 = loop_tmpl ? loop_tmpl : loop_tmpr;
		cad_2d.GetIdLoop_Edge(loop_tmpl, loop_tmpr, edge2);
		loop2 = loop_tmpl ? loop_tmpl : loop_tmpr;
		if (edge1_s_iter == edgesWithPleats.end() && edge2_s_iter != edgesWithPleats.end())
		{
			std::swap(edge1_s_iter, edge2_s_iter);
			std::swap(edge1, edge2);
		}
		EdgeWithPleats newEdgeIdOrdered;
		if (edge1_s_iter != edgesWithPleats.end() && edge2_s_iter == edgesWithPleats.end())
		{
			bool sameDir = true;
			edge1_s_iter->second.divideAnotherEdgeByThis(cad_2d, mesh_2d, edge1, edge2, newEdgeIdOrdered, sameDir);
			for (size_t i = 0, j = 0; i < edge1_s_iter->second.splittedEdgeIdsOrdered.size(); i++)
			{
				if (!edge1_s_iter->second.splittedEdgeValid[i])
					continue;
				aIdECad_Stitch.push_back(std::make_tuple(newEdgeIdOrdered.splittedEdgeIdsOrdered[j++],
					edge1_s_iter->second.splittedEdgeIdsOrdered[i], sameDir));
				// the two edges are stiched inside the same loop, this possibly means a cylinder initialization
				if (loop1 == loop2)
				{
					auto& iter = m_selfStichLoops.find(loop1);
					if (iter == m_selfStichLoops.end())
					{
						m_selfStichLoops.insert(std::make_pair(loop1, SelfStichLoop()));
						iter = m_selfStichLoops.find(loop1);
					}
					iter->second.selfStiches.push_back(aIdECad_Stitch.back());
				}
			}
		}
	} // end for loopEdgePairsBeforePleats

	// 2. triangulation and make stitch-------------------------------------------------------------
	mesh_2d.SetMeshingMode_ElemSize(m_num_coarse_mesh_tri);
	mesh_2d.Meshing(cad_2d);
	std::cout << "Node size : " << mesh_2d.GetVectorAry().size() << std::endl;

	for (unsigned int ist = 0; ist < aIdECad_Stitch.size(); ist++)
	{
		stitch_ary_.AddStitch(cad_2d, mesh_2d, std::get<0>(aIdECad_Stitch[ist]), 
			std::get<1>(aIdECad_Stitch[ist]), std::get<2>(aIdECad_Stitch[ist]));
	}
	std::cout << "stitch size : " << stitch_ary_.aStitch.size() << std::endl;

	// 3. load 3D human body -----------------------------------------------------------------------
	clothHandler_.Clear();
	if (pCT != 0) { delete pCT; pCT = 0; }
	obj_mesh.SetIsNormal(true);
	CSurfaceMeshReader cnt_mesh;
	obj_mesh.Load_Ply("models/wm2_15k.ply");
	cnt_mesh.Load_Ply("models/wm2_cnt.ply");
	double c[3], w[3];
	obj_mesh.GetCenterWidth(c[0], c[1], c[2], w[0], w[1], w[2]);
	double scale = 2 / std::max(std::max(w[0], w[1]), w[2]); // debug, the person should be of 1.65 meters
	obj_mesh.Translate(-c[0], -c[1], -c[2]);  obj_mesh.Scale(scale);  obj_mesh.Rot_Bryant(90, 0, 180);
	cnt_mesh.Translate(-c[0], -c[1], -c[2]);  cnt_mesh.Scale(scale);  cnt_mesh.Rot_Bryant(90, 0, 180);
	obj_mesh.GetCenterWidth(c[0], c[1], c[2], w[0], w[1], w[2]);
	std::cout << "Scaled mesh " << " " << w[0] << " " << w[1] << " " << w[2] << std::endl;
	////
	std::vector<unsigned int> aTri;
	std::vector<double> aXYZ;
	cnt_mesh.GetMesh(aTri, aXYZ);
	clothHandler_.SetObjectMesh(aTri, aXYZ);
	////
	CContactTarget3D_Mesh* pCT1 = new CContactTarget3D_Mesh;
	pCT1->SetMesh(aTri, aXYZ);
	pCT1->BuildBoxel();
	pCT1->SetHole(false);
	pCT = new CContactTarget3D_AdaptiveDistanceField3D();
	double bb[6] = { -1, 1, -1, 1, -1, 1 };
	((CContactTarget3D_AdaptiveDistanceField3D*)pCT)->SetUp(*pCT1, bb);
	((CContactTarget3D_AdaptiveDistanceField3D*)pCT)->BuildMarchingCubeEdge();

	// 4. place 3D cloth pieces -------------------------------------------------------------
	for (size_t iLoop = 0; iLoop < polyLoops.size(); iLoop++)
	{
		const auto& polyLoop = polyLoops[iLoop];
		if (!polyLoop.id_l_add) continue;
		const auto& path = polyPaths[iLoop];
		auto p = polyCenters[iLoop] * pixel2meter;
		auto c = poly3dCenters[iLoop] * pixel2meter;
		auto r = poly3dRots[iLoop].toRotationMatrix3();

		clothHandler_.AddClothPiece(polyLoop.id_l_add, p[0], p[1]);
		clothHandler_.Transform_Cloth_Pan(polyLoop.id_l_add, c[0], c[1], c[2]);
		clothHandler_.Transform_Cloth_Rot(polyLoop.id_l_add, r, true);
		
	} // end for iLoop

	// 4.1 make self-stiched pieces intialized with cylinder
	for (const auto& loop_iter : m_selfStichLoops)
	{
		double dist = 0.;
		for (auto pair : loop_iter.second.selfStiches)
		{
			auto e1 = cad_2d.GetEdge(std::get<0>(pair));
			auto e2 = cad_2d.GetEdge(std::get<1>(pair));
			bool sameDir = std::get<2>(pair);
			if (!sameDir) std::swap(e2.po_s, e2.po_e);
			dist = std::max(dist, (e1.po_s - e2.po_s).Length());
			dist = std::max(dist, (e1.po_e - e2.po_e).Length());
		}
		dist = dist / 2 / M_PI;
		printf("make loop %d a cylinder of radius %f\n", loop_iter.first, dist);
		auto id = loopId2svgIdMap[loop_iter.first];
		auto obj = svgManager->getObjectById(id);
		if (obj->objectType() != svg::SvgAbstractObject::PolyPath)
			throw std::exception("error: not a poly path");
		auto poly = (svg::SvgPolyPath*)obj;
		clothHandler_.SetRadius(loop_iter.first, dist, poly->getCylinderDir());
	} // end for loop_iter

	// 5. finally, other parameters --------------------------------------------------------
	cur_time_ = 0;
	dt_ = 0.005;
	gx_ = 0, gy_ = 0, gz_ = -2;
	//  gx_=0,gy_=0, gz_=-10;     

	// cloth parameters   
	//  cloth_param.stiff_bend = 1.0e-9;
	cloth_param.stiff_bend = 1.0e-10;
	//  cloth_param.stiff_bend = 0.0000;  
	cloth_param.stiff_myu = 0.2;
	cloth_param.stiff_lambda = 0.1;
	cloth_param.rho = 0.02;

	// contact parameters
	contact_param.myu_k = 0.3;
	contact_param.myu_s = 0.5;
	//  contact_param.myu_k = 0.0;  
	//  contact_param.myu_s = 0.0;  
	contact_param.stiff_n = 1;
	contact_param.stiff_f = 1;
	contact_param.offset = 0.01;
	contact_param.offset = 0.015;

	// stitch coeff
	stitch_ary_.SetStiff(1000);
	stitch_ary_.SetDampingCoeff(0.07);
	//  stitch_ary_.SetDampingCoeff(0.0);  
	this->imode_ = CLOTH_INITIAL_LOCATION;

	//if (inum_problem_ == 10)
	//{
	//	cloth_param.stiff_bend = 1.0 - 5;
	//}
	//if (inum_problem_ == 12 || inum_problem_ == 13)
	//{
	//	this->is_detail_ = false;
	//	cloth_param.stiff_bend = 1.5e-3;
	//	cloth_param.stiff_myu = 20;
	//	dt_ = 0.0001;
	//}

	this->InitFieldEqn_fromMsh(cad_2d, mesh_2d);
	this->ClearLinearSystemPreconditioner();
	this->InitDrawer();
	SaveTimeStamp(cad_2d, mesh_2d);
}

