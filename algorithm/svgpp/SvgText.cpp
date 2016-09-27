#include "GL\glew.h"
#include "SvgText.h"
#include "SvgAttribute.h"
namespace svg
{
	SvgText::SvgText()
	{
		m_gl_path_id = glGenPathsNV(1);
		m_font_size = 1;
		m_font = "Sans";
		m_text = "No Text";
		m_total_advance = 0.f;
		updateText();
	}

	SvgText::~SvgText()
	{
		if (m_gl_path_id)
			glDeletePathsNV(m_gl_path_id, 1);
	}

	void SvgText::render()
	{
		glPushMatrix();

		glColor3fv(attribute()->m_color.ptr());

		ldp::Mat3f T = attribute()->m_transfrom;
		ldp::Mat4f M;
		M.eye();
		M(0, 0) = T(0, 0);
		M(0, 1) = T(1, 0);
		M(1, 0) = T(0, 1);
		M(1, 1) = T(1, 1);
		M(0, 3) = T(0, 2);
		M(1, 3) = T(1, 2);
		glMultMatrixf(M.ptr());
		glScalef(1, -1, 1);
		glScalef(m_font_size, m_font_size, m_font_size);

		bool stroking = false;
		bool filling = true;
		FontFacePtr font = requireFontFace(m_font);
		if (stroking) 
		{
			glStencilStrokePathInstancedNV(m_text.size(),
				GL_UNSIGNED_BYTE, m_text.c_str(), font->glyph_base,
				1, ~0,  /* Use all stencil bits */
				GL_TRANSLATE_X_NV, m_hori_shifts.data());
			glCoverStrokePathInstancedNV(m_text.size(),
				GL_UNSIGNED_BYTE, m_text.c_str(), font->glyph_base,
				GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV,
				GL_TRANSLATE_X_NV, m_hori_shifts.data());
		}

		if (filling) {
			/* STEP 1: stencil message into stencil buffer.  Results in samples
			within the message's glyphs to have a non-zero stencil value. */
			glStencilFillPathInstancedNV(m_text.size(),
				GL_UNSIGNED_BYTE, m_text.c_str(), font->glyph_base,
				GL_PATH_FILL_MODE_NV, ~0,  /* Use all stencil bits */
				GL_TRANSLATE_X_NV, m_hori_shifts.data());
			glCoverFillPathInstancedNV(m_text.size(),
				GL_UNSIGNED_BYTE, m_text.c_str(), font->glyph_base,
				GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV,
				GL_TRANSLATE_X_NV, m_hori_shifts.data());

		}

		glPopMatrix();
	}

	void SvgText::renderId()
	{

	}

	void SvgText::updateText()
	{
		FontFacePtr font_face = requireFontFace(m_font);
		m_hori_shifts.resize(m_text.size(), 0.f);
		if (m_hori_shifts.size() > 1)
			glGetPathSpacingNV(GL_ACCUM_ADJACENT_PAIRS_NV,
			m_text.size(), GL_UNSIGNED_BYTE, m_text.c_str(),
			font_face->glyph_base,
			1.0, 1.0,
			GL_TRANSLATE_X_NV,
			m_hori_shifts.data()+1);
		m_total_advance = 0.f;
		if (m_text.size())
			m_total_advance = m_hori_shifts.back() +
			font_face->horizontal_advance[GLubyte(m_text.back())];

		// update bound
		m_bbox_before_transform = ldp::Float4(0, m_total_advance, font_face->y_min, font_face->y_max);
		const ldp::Mat3f& tM = m_attribute->m_transfrom;
		float l = m_bbox_before_transform[0];
		float r = m_bbox_before_transform[1];
		float t = m_bbox_before_transform[2];
		float b = m_bbox_before_transform[3];
		ldp::Float3 bd[4];
		bd[0] = tM * ldp::Float3(l, t, 1);
		bd[1] = tM * ldp::Float3(r, t, 1);
		bd[2] = tM * ldp::Float3(l, b, 1);
		bd[3] = tM * ldp::Float3(r, b, 1);
		resetBound();
		for (int k = 0; k < 4; k++)
			m_bbox = unionBound(ldp::Float2(bd[k][0], bd[k][1]));
	}

	SvgText::FontFacePtr SvgText::requireFontFace(std::string name)
	{
		auto it = m_font_face_map.find(name);
		if (it != m_font_face_map.end())
			return it->second;
		FontFacePtr font = FontFacePtr(new FontFace(name.c_str()));
		m_font_face_map.insert(std::make_pair(name, font));
		return font;
	}

	//////////////////////////////////////////////////////////////////////
	std::map<std::string, SvgText::FontFacePtr> SvgText::m_font_face_map;
	int SvgText::FontFace::gl_path_id = 0;
	SvgText::FontFace::FontFace(const char* name)
	{
		printGLError("Before Creating FontFace");
		if (gl_path_id == 0)
		{
			gl_path_id = glGenPathsNV(1);
			glPathCommandsNV(gl_path_id, 0, NULL, 0, GL_FLOAT, NULL);
			glPathParameterfNV(gl_path_id, GL_PATH_STROKE_WIDTH_NV, 0.1*em_scale);  // 10% of emScale
			glPathParameteriNV(gl_path_id, GL_PATH_JOIN_STYLE_NV, GL_ROUND_NV);
		}
		printGLError("After path template");

		assert(gl_path_id);
		font_name = name;
		target = GL_STANDARD_FONT_NAME_NV;
		num_chars = 256;
		horizontal_advance.resize(num_chars, 0);
		printf("creating font: %s\n", name);
		/* Create a range of path objects corresponding to Latin-1 character codes. */
		glyph_base = glGenPathsNV(num_chars);

		glPathGlyphRangeNV(glyph_base,
			target, name, GL_BOLD_BIT_NV,
			0, num_chars,
			GL_USE_MISSING_GLYPH_NV, gl_path_id,
			em_scale);
		glPathGlyphRangeNV(glyph_base,
			target, "Sans", GL_BOLD_BIT_NV,
			0, num_chars,
			GL_USE_MISSING_GLYPH_NV, gl_path_id,
			em_scale);
		printGLError("After path range1");

		/* Query font and glyph metrics. */
		GLfloat font_data[4];
		glGetPathMetricRangeNV(GL_FONT_Y_MIN_BOUNDS_BIT_NV | GL_FONT_Y_MAX_BOUNDS_BIT_NV,
			glyph_base + ' ', /*count*/1,
			4 * sizeof(GLfloat),
			font_data);
		printGLError("After path range2");

		y_min = font_data[0];
		y_max = font_data[1];
		underline_position = font_data[2];
		underline_thickness = font_data[3];
		glGetPathMetricRangeNV(GL_GLYPH_HORIZONTAL_BEARING_ADVANCE_BIT_NV,
			glyph_base, num_chars,
			0, /* stride of zero means sizeof(GLfloat) since 1 bit in mask */
			horizontal_advance.data());
		printGLError("After path range3");
	}
}