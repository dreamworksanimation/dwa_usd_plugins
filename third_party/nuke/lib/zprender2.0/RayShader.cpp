//
// Copyright 2020 DreamWorks Animation
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//

/// @file zprender/RayShader.cpp
///
/// @author Jonathan Egstad


#include "RayShader.h"
#include "VolumeShader.h"
#include "RayMaterial.h"
#include "SurfaceHandler.h"
#include "RenderContext.h"
#include "ThreadContext.h"
#include "Sampling.h"

#include <DDImage/plugins.h>

#include <dlfcn.h>  // for dlerror

#if __cplusplus <= 201103L
#else
#  include <unordered_map>
#endif

static DD::Image::Lock expand_lock;


namespace zpr {

//------------------------------------------------------------------------------------

//typedef void (*NukeKnobHandler)(DD::Image::Knob* k, Fsr::Pixel& out);

//static void floatKnobHandler(DD::Image::Knob* k, Fsr::Pixel& out)
//{
//}


struct NkKnobMapper
{
    int32_t             nk_num_floats;      //!< If -1 there's n floats
    int32_t             nk_num_doubles;     //!< If -1 there's n doubles
    RayShader::KnobType shader_knob_type;   //!<


    NkKnobMapper(int32_t             nFloats,
                 int32_t             nDoubles,
                 RayShader::KnobType shaderType) :
        nk_num_floats(nFloats),
        nk_num_doubles(nDoubles),
        shader_knob_type(shaderType)
    {
        //
    }
};

#if __cplusplus <= 201103L
typedef std::map<int32_t, NkKnobMapper> NukeKnobTypeMap;
#else
typedef std::unordered_map<int32_t, NkKnobMapper> NukeKnobTypeMap;
#endif

/*! Map of Nuke Knob::ClassID() to RayShader knob type.
*/
static const NukeKnobTypeMap nuke_knob_type_map =
{
    //--------------------------------------------------------------------------------
    // Supported knob mappings:
    { DD::Image::STRING_KNOB,                  NkKnobMapper(0, 0, RayShader::STRING_KNOB) },
    { DD::Image::FILE_KNOB,                    NkKnobMapper(0, 0, RayShader::STRING_KNOB) },
    { DD::Image::CACHED_FILE_KNOB,             NkKnobMapper(0, 0, RayShader::STRING_KNOB) },
    { DD::Image::MULTILINE_STRING_KNOB,        NkKnobMapper(0, 0, RayShader::STRING_KNOB) },
    { DD::Image::MULTILINE_EVAL_STRING_KNOB,   NkKnobMapper(0, 0, RayShader::STRING_KNOB) },
    { DD::Image::TEXT_EDITOR_KNOB,             NkKnobMapper(0, 0, RayShader::STRING_KNOB) },
    { DD::Image::SCRIPT_KNOB,                  NkKnobMapper(0, 0, RayShader::STRING_KNOB) },
    //
    { DD::Image::INT_KNOB,                     NkKnobMapper(0, 0, RayShader::INT_KNOB   ) },
    { DD::Image::BOOL_KNOB,                    NkKnobMapper(0, 0, RayShader::INT_KNOB   ) },
    { DD::Image::RADIO_KNOB,                   NkKnobMapper(0, 0, RayShader::INT_KNOB   ) },
    { DD::Image::ENUMERATION_KNOB,             NkKnobMapper(0, 0, RayShader::INT_KNOB   ) },
    { DD::Image::CASCADING_ENUMERATION_KNOB,   NkKnobMapper(0, 0, RayShader::INT_KNOB   ) },
    //
    { DD::Image::FLOAT_KNOB,                   NkKnobMapper(1, 0, RayShader::FLOAT_KNOB ) },
    { DD::Image::SIZE_KNOB,                    NkKnobMapper(1, 0, RayShader::FLOAT_KNOB ) },
    //
    { DD::Image::DOUBLE_KNOB,                  NkKnobMapper(0, 1, RayShader::DOUBLE_KNOB) },
    { DD::Image::PIXELASPECT_KNOB,             NkKnobMapper(0, 1, RayShader::DOUBLE_KNOB) },
    //
    { DD::Image::ARRAY_KNOB,                   NkKnobMapper(-1, 0, RayShader::FLOATARRAY_KNOB) }, // n floats
    { DD::Image::RESIZABLE_ARRAY_KNOB,         NkKnobMapper(-1, 0, RayShader::FLOATARRAY_KNOB) }, // n floats
    //
    { DD::Image::XY_KNOB,                      NkKnobMapper(0, 2, RayShader::VEC2_KNOB  ) }, // 2 doubles/floats
    { DD::Image::WH_KNOB,                      NkKnobMapper(0, 2, RayShader::VEC2_KNOB  ) }, // 2 doubles/floats
    { DD::Image::UV_KNOB,                      NkKnobMapper(0, 2, RayShader::VEC2_KNOB  ) }, // 2 doubles/floats
    { DD::Image::SCALE_KNOB,                   NkKnobMapper(0, 2, RayShader::VEC2_KNOB  ) }, // 2 doubles
    //
    { DD::Image::XYZ_KNOB,                     NkKnobMapper(3, 0, RayShader::VEC3_KNOB  ) }, // 3 floats
    //
    { DD::Image::BOX3_KNOB,                    NkKnobMapper(6, 0, RayShader::EMPTY_KNOB ) }, // 6 floats - TODO: support
    { DD::Image::BBOX_KNOB,                    NkKnobMapper(0, 4, RayShader::EMPTY_KNOB ) }, // 4 doubles - TODO: support
    //
    { DD::Image::COLOR_KNOB,                   NkKnobMapper(3, 0, RayShader::COLOR3_KNOB) }, // 3 doubles/floats
    { DD::Image::ACOLOR_KNOB,                  NkKnobMapper(4, 0, RayShader::COLOR4_KNOB) }, // 4 doubles/floats
    //
    { DD::Image::TRANSFORM2D_KNOB,             NkKnobMapper(16, 0, RayShader::MAT4_KNOB ) }, // 16 floats (Mat4f) - TODO: support
    { DD::Image::AXIS_KNOB,                    NkKnobMapper(16, 0, RayShader::MAT4_KNOB ) }, // 16 floats (Mat4f) - TODO: support
    //
    { DD::Image::CHANNEL_MASK_KNOB,            NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) }, // TODO: support?
    { DD::Image::CHANNEL_KNOB,                 NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) }, // TODO: support?
    { DD::Image::INPUTONLY_CHANNEL_MASK_KNOB,  NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) }, // TODO: support?
    { DD::Image::INPUTONLY_CHANNEL_KNOB,       NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) }, // TODO: support?

#if 0
    //--------------------------------------------------------------------------------
    // Unsupported knobs:
    { DD::Image::FORMAT_KNOB,                  NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    //
    { DD::Image::ONEVIEW_KNOB,                 NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    { DD::Image::MULTIVIEW_KNOB,               NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    { DD::Image::VIEWVIEW_KNOB,                NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    { DD::Image::VIEWPAIR_KNOB,                NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    //
    { DD::Image::CUSTOM_KNOB,                  NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    //
    { DD::Image::PULLDOWN_KNOB,                NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    { DD::Image::PYPULLDOWN_KNOB,              NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    { DD::Image::MULTIARRAY_KNOB,              NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    { DD::Image::LIST_KNOB,                    NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },

    { DD::Image::CP_KNOB,                      NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    { DD::Image::MENU_KNOB,                    NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    { DD::Image::PASSWORD_KNOB,                NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    { DD::Image::TABLE_KNOB,                   NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    { DD::Image::CONTROL_POINT_COLLECTION_KNOB,NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    { DD::Image::DYNAMIC_BITMASK_KNOB,         NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    { DD::Image::META_KEY_FRAME_KNOB,          NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    { DD::Image::POSITIONVECTOR_KNOB,          NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },

    { DD::Image::SIMPLE_ARRAY_KNOB,            NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    { DD::Image::DISABLE_KNOB,                 NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    { DD::Image::FREETYPE_KNOB,                NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
    { DD::Image::EDITABLE_ENUMERATION_KNOB,    NkKnobMapper(0, 0, RayShader::EMPTY_KNOB ) },
#endif
};


//------------------------------------------------------------------------------------


//! Return the string version of the type enum.
/*static*/
const char*
RayShader::typeString(KnobType type)
{
    switch (type)
    {
        default:
        case EMPTY_KNOB:      return "none";       break;
        //
        case STRING_KNOB:     return "string";     break;
        case INT_KNOB:        return "int";        break;
        case FLOAT_KNOB:      return "float";      break;
        case DOUBLE_KNOB:     return "double";     break;
        //
        case COLOR2_KNOB:     return "color2";     break;
        case COLOR3_KNOB:     return "color3";     break;
        case COLOR4_KNOB:     return "color4";     break;
        //
        case VEC2_KNOB:       return "vec2";       break;
        case VEC3_KNOB:       return "vec3";       break;
        case VEC4_KNOB:       return "vec4";       break;
        //
        case MAT4_KNOB:       return "mat4";       break;
        //
        case FLOATARRAY_KNOB: return "floatarray"; break;
        case VEC2ARRAY_KNOB:  return "vec2array";  break;
        case VEC3ARRAY_KNOB:  return "vec3array";  break;
        case VEC4ARRAY_KNOB:  return "vec4array";  break;
        //
        case PIXEL_KNOB:      return "pixel";      break;
    }
}


//------------------------------------------------------------------------------------


static Fsr::Vec4f vec4_zero(0.0f, 0.0f, 0.0f, 0.0f);
static Fsr::Vec4f  vec4_one(1.0f, 1.0f, 1.0f, 1.0f);


/*!
*/
RayShader::InputKnob::InputKnob() :
    name(""),
    type(EMPTY_KNOB),
    data(NULL),
    default_value(NULL),
    shader(NULL),
    output_index(-1)
{
    //
}


/*!
*/
RayShader::InputKnob::InputKnob(const char* knob_name,
                                KnobType    data_type,
                                const char* default_val) :
    name((knob_name) ? knob_name : ""),
    type(data_type),
    data(NULL),
    default_value(default_val),
    shader(NULL),
    output_index(-1)
{
    //
}

#define DBL_STR_SIZE 64

/*!
*/
std::string
RayShader::InputKnob::getText() const
{
    switch (type)
    {
        default:
        case EMPTY_KNOB: return std::string("");

        case STRING_KNOB: return (std::string)*this;

        case INT_KNOB: {
            char buf[DBL_STR_SIZE];
            std::snprintf(buf, DBL_STR_SIZE, "%d", (int32_t)*this);
            return std::string(buf); }

        case FLOAT_KNOB: {
            char buf[DBL_STR_SIZE];
            std::snprintf(buf, DBL_STR_SIZE, "%.20g", (float)*this);
            return std::string(buf); }

        case DOUBLE_KNOB: {
            char buf[DBL_STR_SIZE];
            std::snprintf(buf, DBL_STR_SIZE, "%.20g", (double)*this);
            return std::string(buf); }

        case COLOR2_KNOB:
        case VEC2_KNOB: {
            const Fsr::Vec2f& v = Fsr::Vec2f(*this);
            char buf[DBL_STR_SIZE*2];
            std::snprintf(buf, DBL_STR_SIZE*2, "%.20g %.20g", v.x, v.y);
            return std::string(buf); }

        case COLOR3_KNOB:
        case VEC3_KNOB: {
            const Fsr::Vec3f& v = Fsr::Vec3f(*this);
            char buf[DBL_STR_SIZE*3];
            std::snprintf(buf, DBL_STR_SIZE*3, "%.20g %.20g %.20g", v.x, v.y, v.z);
            return std::string(buf); }

        case COLOR4_KNOB:
        case VEC4_KNOB: {
            const Fsr::Vec4f& v = Fsr::Vec4f(*this);
            char buf[DBL_STR_SIZE*4];
            std::snprintf(buf, DBL_STR_SIZE*4, "%.20g %.20g %.20g %.20g", v.x, v.y, v.z, v.w);
            return std::string(buf); }

        case MAT4_KNOB: {
            const Fsr::Mat4d& m = Fsr::Mat4d(*this);
            char buf[DBL_STR_SIZE*16];
            std::snprintf(buf, DBL_STR_SIZE*16,
                            "%.20g %.20g %.20g %.20g "
                            "%.20g %.20g %.20g %.20g "
                            "%.20g %.20g %.20g %.20g "
                            "%.20g %.20g %.20g %.20g",
                                m.a00, m.a10, m.a20, m.a30,
                                m.a01, m.a11, m.a21, m.a31,
                                m.a02, m.a12, m.a22, m.a32,
                                m.a03, m.a13, m.a23, m.a33);
            return std::string(buf); }

        case FLOATARRAY_KNOB: return std::string("");
        case VEC2ARRAY_KNOB:  return std::string("");
        case VEC3ARRAY_KNOB:  return std::string("");
        case VEC4ARRAY_KNOB:  return std::string("");

        case PIXEL_KNOB:      return std::string("");
    }
}


/*! Print the name, type and contents of knob to stream.
*/
void
RayShader::InputKnob::print(std::ostream& o) const
{
    o << name << "(" << typeString(type) << ")[" << getText() << "]";
}


/*!
*/
void
RayShader::InputKnob::setValue(const char* value)
{
    if (!value)
        return; // don't crash
    else if (!data)
    {
        std::cerr << "setValue(" << value << ") on input knob '" << name << "'";
        std::cerr << " ignored, knob has no assigned data pointer";
        std::cerr << std::endl;
        return; // don't crash
    }

    //std::cout << "        " << name << "(" << type << ")::setValue(" << value << ") data=" << data << std::endl;
    switch (type)
    {
        default:
        case EMPTY_KNOB:
            break;
        //
        case STRING_KNOB:
        {
            std::string& v = *static_cast<std::string*>(data);
            v = value;
            break;
        }
        //
        case INT_KNOB:
        {
            int32_t& v = *static_cast<int32_t*>(data);
            v = ::atoi(value);
            break;
        }
        case FLOAT_KNOB:
        {
            float& v = *static_cast<float*>(data);
            v = float(::atof(value));
            break;
        }
        case DOUBLE_KNOB:
        {
            double& v = *static_cast<double*>(data);
            v = ::atof(value);
            break;
        }
        //
        case COLOR2_KNOB:
        case VEC2_KNOB:
        {
            Fsr::Vec2f& v = *static_cast<Fsr::Vec2f*>(data);
            if (std::sscanf(value, "%f %f", &v.x, &v.y) != 2)
            {
                std::sscanf(value, "%f, %f", &v.x, &v.y);
            }
            break;
        }
        case COLOR3_KNOB:
        case VEC3_KNOB:
        {
            Fsr::Vec3f& v = *static_cast<Fsr::Vec3f*>(data);
            if (std::sscanf(value, "%f %f %f", &v.x, &v.y, &v.z) != 3)
            {
                std::sscanf(value, "%f, %f, %f", &v.x, &v.y, &v.z);
            }
            break;
        }
        case COLOR4_KNOB:
        case VEC4_KNOB:
        {
            Fsr::Vec4f& v = *static_cast<Fsr::Vec4f*>(data);
            if (std::sscanf(value, "%f %f %f %f", &v.x, &v.y, &v.z, &v.w) != 4)
            {
                std::sscanf(value, "%f, %f, %f, %f", &v.x, &v.y, &v.z, &v.w);
            }
            break;
        }
        case MAT4_KNOB:
        {
            Fsr::Mat4d& m = *static_cast<Fsr::Mat4d*>(data);
            if (std::sscanf(value, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                            &m.a00, &m.a10, &m.a20, &m.a30,
                            &m.a01, &m.a11, &m.a21, &m.a31,
                            &m.a02, &m.a12, &m.a22, &m.a32,
                            &m.a03, &m.a13, &m.a23, &m.a33) != 16)
            {
                std::sscanf(value, "%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf",
                            &m.a00, &m.a10, &m.a20, &m.a30,
                            &m.a01, &m.a11, &m.a21, &m.a31,
                            &m.a02, &m.a12, &m.a22, &m.a32,
                            &m.a03, &m.a13, &m.a23, &m.a33);
            }
            break;
        }
        //
        case FLOATARRAY_KNOB:
// TODO: support!
            break;
        case VEC2ARRAY_KNOB:
// TODO: support!
            break;
        case VEC3ARRAY_KNOB:
// TODO: support!
            break;
        case VEC4ARRAY_KNOB:
// TODO: support!
            break;
        //
        case PIXEL_KNOB:
        {
            // We don't store the channel values yet, just the channel list:
            Fsr::Pixel& p = *static_cast<Fsr::Pixel*>(data);
// TODO: parse the channel names and use the DDImage channel findChannel() method to get the Channels.
            const std::string str(value);
            if      (str == "rgb" ) p.setChannels(DD::Image::Mask_RGB  );
            else if (str == "rgba") p.setChannels(DD::Image::Mask_RGBA );
            else if (str == "r"   ) p.setChannels(DD::Image::Mask_Red  );
            else if (str == "g"   ) p.setChannels(DD::Image::Mask_Green);
            else if (str == "b"   ) p.setChannels(DD::Image::Mask_Blue );
            else if (str == "a"   ) p.setChannels(DD::Image::Mask_Alpha);

            //std::cout << "        " << name << "(" << type << ")::setValue(" << value << ")" << ", channels=" << p.channels << std::endl;
            break;
        }
    }
    //std::cout << "  InputKnob::setValue() " << *this << std::endl;
}


/*! Copy value from an Op knob, if types match, return true on success.

    If the data type of the Op knob matches or can be converted
    the value is copied.
*/
bool
RayShader::InputKnob::setValue(const DD::Image::Knob*          op_knob,
                               const DD::Image::OutputContext& op_context)
{
    if (!op_knob)
        return false;

const bool print_warning = true;//false;

    if (!data)
    {
        if (print_warning)
        {
            std::cerr << "setValue('" << op_knob->name() << "') on input knob '" << name << "'";
            std::cerr << " ignored, knob has no assigned data pointer";
            std::cerr << std::endl;
        }
        return false; // don't crash
    }

    //std::cout << "setValue('" << op_knob->name() << "') on input knob '" << name << "'";
    //std::cout << ", ClassID=" << op_knob->ClassID();
    //std::cout << std::endl;

    // Is there a mapping for this Nuke knob type?
    const NukeKnobTypeMap::const_iterator mapping = nuke_knob_type_map.find(op_knob->ClassID());
    if (mapping == nuke_knob_type_map.end())
    {
        if (print_warning)
        {
            std::cerr << "setValue('" << op_knob->name() << "') on input knob '" << name << "'";
            std::cerr << " ignored, Nuke knob class " << op_knob->Class() << " cannot be handled.";
            std::cerr << std::endl;
        }
        return false; // can't copy data
    }

    const NkKnobMapper& mapper = mapping->second;
    if (mapper.shader_knob_type != this->type)
    {
        if (print_warning)
        {
            std::cerr << "setValue('" << op_knob->name() << "') on input knob '" << name << "'";
            std::cerr << " ignored, Nuke knob class " << op_knob->Class() << " is not supported";
            std::cerr << " by this knob's '" << typeString(mapper.shader_knob_type) << "' type.";
            std::cerr << std::endl;
        }
        return false; // can't copy data
    }

    DD::Image::Hash dummy_hash;
    DD::Image::Knob* k = const_cast<DD::Image::Knob*>(op_knob);

    //std::cout << "  mapped: shader_knob_type=" << typeString(mapper.shader_knob_type);
    //std::cout << ", nNkFloats=" << mapper.nk_num_floats;
    //std::cout << ", nNkDoubles=" << mapper.nk_num_doubles;
    switch (mapper.shader_knob_type)
    {
        default:
        case EMPTY_KNOB:
        case PIXEL_KNOB:
            return false;
        //
        case STRING_KNOB:
        {
            assert(k->stringKnob()); // shouldn't happen...

            const char* s = NULL;
            k->store(DD::Image::StringPtr, &s, dummy_hash, op_context);
            if (s)
                *static_cast<std::string*>(data) = s;
            else
                *static_cast<std::string*>(data) = "";
            return true;
        }
        //
        case INT_KNOB:
            k->store(DD::Image::IntPtr, data, dummy_hash, op_context);
            return true;
        case FLOAT_KNOB:
            k->store(DD::Image::FloatPtr, data, dummy_hash, op_context);
            return true;
        case DOUBLE_KNOB:
            k->store(DD::Image::DoublePtr, data, dummy_hash, op_context);
            return true;
        //
        case COLOR2_KNOB:
        case VEC2_KNOB:
        {
            double vals[2];
            k->store(DD::Image::DoublePtr, vals, dummy_hash, op_context);
            static_cast<Fsr::Vec2f*>(data)->set(float(vals[0]), float(vals[1]));
            return true;
        }
        case COLOR3_KNOB:
        case VEC3_KNOB:
        {
            double vals[3];
            k->store(DD::Image::DoublePtr, vals, dummy_hash, op_context);
            static_cast<Fsr::Vec3f*>(data)->set(float(vals[0]), float(vals[1]), float(vals[2]));
            return true;
        }
        case COLOR4_KNOB:
        case VEC4_KNOB:
        {
            double vals[4];
            k->store(DD::Image::DoublePtr, vals, dummy_hash, op_context);
            static_cast<Fsr::Vec4f*>(data)->set(float(vals[0]), float(vals[1]), float(vals[2]), float(vals[3]));
            return true;
        }
        //
        case MAT4_KNOB:
        {
            float vals[16];
            k->store(DD::Image::FloatPtr, vals, dummy_hash, op_context);
            Fsr::Mat4d& m = *static_cast<Fsr::Mat4d*>(data);
            m.setTo(double(vals[ 0]), double(vals[ 1]), double(vals[ 2]), double(vals[ 3]),
                    double(vals[ 4]), double(vals[ 5]), double(vals[ 6]), double(vals[ 7]),
                    double(vals[ 8]), double(vals[ 9]), double(vals[10]), double(vals[11]),
                    double(vals[12]), double(vals[13]), double(vals[14]), double(vals[15]));
            return true;
        }
        //
        /*
          TODO: support
            { DD::Image::ARRAY_KNOB,           NkKnobMapper(-1, 0, RayShader::FLOATARRAY_KNOB) }, // n floats
            { DD::Image::RESIZABLE_ARRAY_KNOB, NkKnobMapper(-1, 0, RayShader::FLOATARRAY_KNOB) }, // n floats
        */
        case FLOATARRAY_KNOB:
            break;
        case VEC2ARRAY_KNOB:
            break;
        case VEC3ARRAY_KNOB:
            break;
        case VEC4ARRAY_KNOB:
            break;
    }
    //std::cout << std::endl;

    //std::cout << "  InputKnob::setValue() " << *this << std::endl;

    return false; // unhandled
}


void
RayShader::InputKnob::setString(const std::string& value)
{
    if (data && type == STRING_KNOB)
        *static_cast<std::string*>(data) = value;
}
void
RayShader::InputKnob::setString(const char* value) 
{
    if (data && type == STRING_KNOB)
        *static_cast<std::string*>(data) = value;
}

void
RayShader::InputKnob::setInt(int value)
{
    if (data && type == INT_KNOB)
        *static_cast<int32_t*>(data) = value;
}
void RayShader::InputKnob::setBool(bool value) { setInt(value); }

void
RayShader::InputKnob::setFloat(float value)
{
    if (data && type == FLOAT_KNOB)
        *static_cast<float*>(data) = value;
}

void
RayShader::InputKnob::setDouble(double value)
{
    if (data && type == DOUBLE_KNOB)
        *static_cast<double*>(data) = value;
}

void
RayShader::InputKnob::setVec2f(const Fsr::Vec2f& value)
{
    if (data && (type == VEC2_KNOB || type == COLOR2_KNOB))
        *static_cast<Fsr::Vec2f*>(data) = value;
}
void
RayShader::InputKnob::setVec3f(const Fsr::Vec3f& value)
{
    if (data && (type == VEC3_KNOB || type == COLOR3_KNOB))
        *static_cast<Fsr::Vec3f*>(data) = value;
}
void
RayShader::InputKnob::setVec4f(const Fsr::Vec4f& value)
{
    if (data && (type == VEC4_KNOB || type == COLOR4_KNOB))
        *static_cast<Fsr::Vec4f*>(data) = value;
}

void
RayShader::InputKnob::setMat4d(const Fsr::Mat4d& value)
{
    if (data && type == INT_KNOB)
        *static_cast<Fsr::Mat4d*>(data) = value;
}


//------------------------------------------------------------------------------------


/*! Print the name, type and contents of knob to stream.
*/
void
RayShader::OutputKnob::print(std::ostream& o) const
{
    o << name << "(" << typeString(type) << ")";//[" << getText() << "]";
}


//------------------------------------------------------------------------------------

    // See if we recognize any of the outputs.
    // We can handle generic 'surface' or 'displacement' output which are assumed
    // to connect to 'Usd*' shaders like UsdPreviewSurface, UsdUVTexture, etc.
    //
    // TODO: need to also handle zpr networks as first-class outputs!
    //



//!
/*static*/ const char* RayShader::zpClass() { return "zpRayShader"; }

static const RayShader::InputKnob      m_empty_input;
static const RayShader::OutputKnob     m_empty_output;
static const RayShader::InputKnobList  m_default_inputs = {};
static const RayShader::OutputKnobList m_default_outputs =
{
    {RayShader::OutputKnob("surface", RayShader::PIXEL_KNOB)}
};


/*!
*/
RayShader::RayShader() :
    m_inputs(m_default_inputs),
    m_outputs(m_default_outputs),
    m_valid(false)
{
    for (uint32_t i=0; i < m_inputs.size(); ++i)
        m_input_name_map[m_inputs[i].name] = i;
    for (uint32_t i=0; i < m_outputs.size(); ++i)
        m_output_name_map[m_outputs[i].name] = i;
}


/*!
*/
RayShader::RayShader(const InputKnobList&  inputs,
                     const OutputKnobList& outputs) :
    m_inputs(inputs),
    m_outputs(outputs),
    m_valid(false)
{
    for (uint32_t i=0; i < m_inputs.size(); ++i)
        m_input_name_map[m_inputs[i].name] = i;
    for (uint32_t i=0; i < m_outputs.size(); ++i)
        m_output_name_map[m_outputs[i].name] = i;
}


/*! Print input and output knob values to stream.
*/
void
RayShader::print(std::ostream& o) const
{
    o << m_name << ":" << std::endl;
    o << "  inputs:" << std::endl;
    for (uint32_t i=0; i < m_inputs.size(); ++i)
        o << "    " << m_inputs[i] << std::endl;
    o << "  outputs:" << std::endl;
    for (uint32_t i=0; i < m_outputs.size(); ++i)
        o << "    " << m_outputs[i] << std::endl;
}


/*! Return a static list of input knobs for this shader.
    Base class returns an empty list.
*/
/*virtual*/
const RayShader::InputKnobList&
RayShader::getInputKnobDefinitions() const
{
    return m_default_inputs;
}


/*! Return a static list of output knobs for this shader.
    Base class returns only the 'primary' output.
*/
/*virtual*/
const RayShader::OutputKnobList&
RayShader::getOutputKnobDefinitions() const
{
    return m_default_outputs;
}


/*! Returns input knob or NULL if not available.
*/
RayShader::InputKnob*
RayShader::getInputKnob(uint32_t input) const
{
    if (input >= m_inputs.size())
        return NULL;
    return const_cast<InputKnob*>(&m_inputs[input]);
}
RayShader::InputKnob*
RayShader::getInputKnob(const std::string& input_name) const
{
    const int32_t input = getInputIndex(input_name);
    return (input < 0) ? NULL : const_cast<InputKnob*>(&m_inputs[input]);
}


/*! Returns output knob or NULL if not available.
*/
RayShader::OutputKnob*
RayShader::getOutputKnob(uint32_t output) const
{
    if (output >= m_outputs.size())
        return NULL;
    return const_cast<OutputKnob*>(&m_outputs[output]);
}
RayShader::OutputKnob*
RayShader::getOutputKnob(const std::string& output_name) const
{
    const int32_t output = getOutputIndex(output_name);
    return (output < 0) ? NULL : const_cast<OutputKnob*>(&m_outputs[output]);
}


/*! Return a named input's index or -1 if not found.
*/
int32_t
RayShader::getInputIndex(const std::string& input_name) const
{
    if (input_name.empty())
        return -1;
    const KnobNameMap::const_iterator it = m_input_name_map.find(input_name);
    return (it == m_input_name_map.end()) ? -1 : it->second;
}


/*! Return a named output's index or -1 if not found.
*/
int32_t
RayShader::getOutputIndex(const std::string& output_name) const
{
    if (output_name.empty())
        return -1;
    const KnobNameMap::const_iterator it = m_output_name_map.find(output_name);
    return (it == m_output_name_map.end()) ? -1 : it->second;
}


/*! Returns shader pointer for input.
    May be NULL if there's no input or no connection.
*/
RayShader*
RayShader::getInputShader(uint32_t input) const
{
    if (input >= m_inputs.size())
        return NULL;
    return m_inputs[input].shader;
}


/*! Returns true if input can be connected to another RayShader's named output.

    Base class tests if the shader has the named output and its type matches
    the input's.
*/
/*virtual*/
bool
RayShader::canConnectInputTo(uint32_t    input,
                             RayShader*  shader,
                             const char* output_name)
{
    if (!shader || shader == this || input >= m_inputs.size())
        return false;

    const int output_index = shader->getOutputIndex(output_name);
    if (output_index == -1)
        return false; // no output match

    return true;
}


/*! Attempt to connect input to another RayShader's named output.

    The virtual method \b canConnectInputTo() is called on this shader which
    returns true if the connection is allowed.

    If connection is allowed, the virtual method _connectInput() is
    called to allow sublasses to do special things with the RayShader
    input like hook up additional shaders to the input.
*/
bool
RayShader::connectInput(uint32_t    input,
                        RayShader*  shader,
                        const char* output_name)
{
    if (!shader)
    {
        std::cerr << "        " << m_name << "::connectInput(" << input << ") ERROR, null input shader" << std::endl;
        return false;
    }
    else if (shader == this)
    {
        std::cerr << "        " << m_name << "::connectInput(" << input << ") ERROR, cannot connect shader to itself" << std::endl;
        return false;
    }
    else if (input >= m_inputs.size())
    {
        std::cerr << "        " << m_name << "::connectInput(" << input << ") ERROR, input index out of range" << std::endl;
        return false;
    }

    const int output_index = shader->getOutputIndex(output_name);
    if (output_index == -1)
    {
        //std::cout << "        " << m_name << "::connectInput(" << input << ") FAILED connection to '" << output_name << "'" << std::endl;
        return false; // no output match
    }

    // Connect it up:
    m_inputs[input].shader       = shader;
    m_inputs[input].output_index = output_index;
    //std::cout << "        " << m_name << "::connectInput(" << input << ") shader=" << shader << ", name='" << output_name << "'" << std::endl;

    // Allow subclasses to do their own connection logic:
    _connectInput(input, shader, output_name);

    return true;
}


/*! Subclass implementation of connectInput().
	Base class does nothing.
*/
/*virtual*/
void
RayShader::_connectInput(uint32_t    input,
                         RayShader*  shader,
                         const char* output_name)
{
    /* Do nothing */
}


/*! Convenience method to assign the data value target of a named InputKnob.
    If default value string is provided it's updated in knob.
    If default value is non-null the assigned data pointer value is set to the
    default_value.
    Returns true if data pointer was assigned and set successfully.
*/
bool
RayShader::assignInputKnob(const char* input_name,
                           void*       data,
                           const char* default_val)
{
    InputKnob* k = getInputKnob(input_name);
    if (!k || !data)
        return false;

    if (default_val)
        k->default_value = default_val;

    k->data = data;
    if (k->default_value)
        k->setValue(k->default_value);

    return true;
}


/*!
*/
void
RayShader::setInputValue(uint32_t    input,
                         const char* value)
{
    if (!value || input >= m_inputs.size())
        return; // don't crash
    m_inputs[input].setValue(value);
}


/*!
*/
void
RayShader::setInputValue(const char* input_name,
                         const char* value)
{
    InputKnob* knob = getInputKnob(input_name);
    if (knob)
        knob->setValue(value);
}


/*! Set input knob value from an Op knob, return true if achieved.

    If the data type of the Op knob matches or can be converted
    the value is copied.
*/
bool
RayShader::setInputValue(const char*                     input_name,
                         const DD::Image::Knob*          op_knob,
                         const DD::Image::OutputContext& op_context)
{
    InputKnob* knob = getInputKnob(input_name);
    if (knob)
        return knob->setValue(op_knob, op_context);
    return false;
}


//-----------------------------------------------------------------------------


/*!
*/
/*virtual*/
void
RayShader::validateShader(bool                 for_real,
                          const RenderContext& rtx)
{
    if (m_valid)
        return;

    const uint32_t nInputs = (uint32_t)m_inputs.size();
    for (uint32_t i=0; i < nInputs; ++i)
        if (getInputShader(i))
            getInputShader(i)->validateShader(for_real, rtx);

    m_valid = true;
}


/*!
*/
/*virtual*/
void
RayShader::getActiveTextureBindings(std::vector<InputBinding*>& texture_bindings)
{
    const uint32_t nInputs = (uint32_t)m_inputs.size();
    for (uint32_t i=0; i < nInputs; ++i)
        if (getInputShader(i))
            getInputShader(i)->getActiveTextureBindings(texture_bindings);
}


/*! Surface evaluation returns the radiance and aovs from this RayShader
    given an intersection point and incoming ray in the RayShaderContext.

    Base class sets sets the output color to 18% grey, full opacity.
*/
/*virtual*/
void
RayShader::evaluateSurface(RayShaderContext& stx,
                           Fsr::Pixel&       out)
{
    out.rgba().set(0.18f, 0.18f, 0.18f, 1.0f);
}


/*! Surface displacement evaluation call.
*/
/*virtual*/
void
RayShader::evaluateDisplacement(RayShaderContext& stx,
                                Fsr::Pixel&       out)
{
    // do nothing
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------


/*  Map of already loaded ShaderDescriptions to speed up lookups.

    Use of this static singleton class allows m_dso_map to be shared between
    plugins that statically link against libzprender.

    Not clear why this is any functionally different than declaring a static
    RayShaderDescMap, but I'm sure smarter folk than I know. Probably something
    to do with when the static object is created in the process memory.
*/
class DsoMap
{
  private:
#if __cplusplus <= 201103L
    typedef std::map<std::string, const RayShader::ShaderDescription*> RayShaderDescMap;
#else
    typedef std::unordered_map<std::string, const RayShader::ShaderDescription*> RayShaderDescMap;
#endif
    RayShaderDescMap m_dso_map;


  public:
    //!
    DsoMap()
    {
        //std::cout << "DsoMap::ctor(): RayShaderDescMap singleton=" << &m_dso_map << std::endl;
        //m_dso_map[std::string("Foo")] = NULL;
    }

    /*! Return the static DsoMap singleton.

        For some magical reason this works across statically linked plugins, but putting
        'static RayShaderDescMap m_dso_map;' as a global in the cpp file doesn't...
        Probably something to do with when the static object is created in the process memory.
    */
    static RayShaderDescMap* dsoMap()
    {
        static DsoMap m_instance;
        //std::cout << "  DsoMap::instance(): singleton=" << &m_instance.m_dso_map << std::endl;
        return &m_instance.m_dso_map;
    }

    //!
    static const RayShader::ShaderDescription* find(const std::string& shader_class)
    {
        if (shader_class.empty())
            return NULL;
        DD::Image::Guard guard(expand_lock); // just in case...
        RayShaderDescMap::const_iterator it = dsoMap()->find(shader_class);
        if (it != dsoMap()->end())
            return it->second;
        return NULL; // not found
    }
    
    //!
    static void add(const std::string&                  shader_class,
                    const RayShader::ShaderDescription* desc)
    {
        if (shader_class.empty() || !desc)
            return;
        DD::Image::Guard guard(expand_lock); // just in case...
        (*dsoMap())[shader_class] = desc;
    }

};


//--------------------------------------------------------------------------------------------------



/*static*/
RayShader*
RayShader::create(const ShaderDescription& node_description)
{
    return create(node_description.shaderClass());
}


/*! Create a RayShader instance based on the type name ('abcProcedural', 'PerspectiveCamera', etc.)
    Calling code takes ownership of returned pointer.
*/
/*static*/
RayShader*
RayShader::create(const char* shader_class)
{
    if (!shader_class || !shader_class[0])
        return NULL;
    //std::cerr << "zpr::RayShader::create('" << shader_class << "')" << std::endl;

    // Get the description by name:
    const RayShader::ShaderDescription* desc = find(shader_class);
    if (!desc)
        return NULL; // can't find plugin...

    // Allocate a new one and return it:
    RayShader* dso = desc->builder_method();
    if (!dso)
    {
        std::cerr << "zpr::RayShader::create(): error, cannot allocate new shader of type '" << shader_class << "'" << std::endl;
        return NULL;
    }
    //std::cerr << "loaded description '" << desc->shaderClass() << "', dso=" << dso << std::endl;

    return dso;
}


/*! Constructor sets name and label to same value.
*/
RayShader::ShaderDescription::ShaderDescription(const char*   shader_class,
                                                PluginBuilder builder) :
    m_shader_class(shader_class),
    builder_method(builder)
{
    //std::cout << "  zpr::RayShader::ShaderDescription::ctor(" << shader_class << ")" << std::endl;

    // DD::Image::Description.h:
    //  const char* compiled;   // Date and DD_IMAGE_VERSION_LONG this was compiled for
    //  const char* plugin;     // Set to the plugin filename
    //  License*    license;    // If non-null, license check is run

    // No need for license checks, although this could be leveraged to stop
    // DD::Image from loading a Fuser plugin accidentally:
    DD::Image::Description::license = NULL;

    // Register the plugin callback - this is called when the plugin is loaded:
    ShaderDescription::ctor(pluginBuilderCallback);

    // Update compiled string to use Fuser version rather than kDDImageVersion:
    DD::Image::Description::compiled = __DATE__ " for Fuser-" FuserVersion;
}


/*! Called when the plugin .so is first loaded.
    This adds the plugin class to the map of loaded dsos so that we don't need
    to search or load the .so again.
*/
/*static*/
void
RayShader::ShaderDescription::pluginBuilderCallback(DD::Image::Description* desc)
{
    if (!desc)
        return; // don't crash...

    const RayShader::ShaderDescription* dso_desc = static_cast<const RayShader::ShaderDescription*>(desc);

    const char* shader_class = dso_desc->shaderClass();
    assert(shader_class && shader_class[0]);

    //std::cout << "  zpr::RayShader::ShaderDescription::pluginBuilderCallback(" << dso_desc << "):";
    //std::cout << " shader_class='" << shader_class << "'" << std::endl;

    // Add to dso map if it doesn't already exist.
    // Statically linked plugins will cause the libFuser built in descriptions
    // to call this repeatedly, so ignore any repeats:
    if (!DsoMap::find(std::string(shader_class)))
    {
        DsoMap::add(std::string(shader_class), dso_desc);
        //std::cout << "    (pluginBuilderCallback) adding '" << shader_class << "'=" << dso_desc << std::endl;
    }
}


/*! Find a dso description by name.

    If it's been loaded before it quickly returns an existing cached
    ShaderDescription, otherwise it prepends 'zpr' to the start of the name
    (ie 'zprMyShaderClass')  before searching the plugin paths for a
    matching plugin filename.

    Returns NULL if not found.
*/
/*static*/
const RayShader::ShaderDescription*
RayShader::ShaderDescription::find(const char* shader_class)
{
    if (!shader_class || !shader_class[0])
        return NULL;  // just in case...
    const std::string dso_name(shader_class);

    //std::cout << "zpr::RayShader::ShaderDescription::find('" << dso_name << "') dso_map=" << DsoMap::dsoMap() << std::endl;

    // Search for existing dso using the base shaderClass() name
    // (ie UsdIO, UsdaIO, MeshPrim, etc)
    const RayShader::ShaderDescription* dso_desc = DsoMap::find(dso_name);
    if (dso_desc)
        return dso_desc;

    // Not found, prepend 'zpr' to name and search the plugin paths for
    // the plugin dso file (ie zprBaseSurface.so, zprDisplacement.tcl, etc)
    std::string plugin_name("zpr");
    plugin_name += dso_name;

    // Use the stock DDImage plugin load method, which supports .tcl redirectors.
    // It's important because we're relying on .tcl directors to handle aliasing
    // in several plugins:
    // NOTE: DD::Image::plugin_load() says that it returns NULL if a plugin is
    // not loaded but that does not appear to be the case. It returns the path
    // to the plugin it *attempted* to load, but only by checking plugin_error()
    // can we tell if dlopen() failed and what was returned in dlerror()
    const char* plugin_path = DD::Image::plugin_load(plugin_name.c_str());
    if (!plugin_path || !plugin_path[0])
    {
        std::cerr << "zpr::RayShader::ShaderDescription::find('" << plugin_name << "') ";
        std::cerr << "error: plugin not found." << std::endl;
        return NULL;  // plugin not found!
    }
    // Was there a dlerror() on load?
    if (DD::Image::plugin_error())
    {
        std::cerr << "zpr::RayShader::ShaderDescription::find('" << plugin_name << "') ";
        std::cerr << "error: plugin not loaded, dlopen error '" << DD::Image::plugin_error() << "'" << std::endl;
        return NULL;  // plugin not found!
    }

    // Plugin found and loaded, return the pointer that was added to the map:
    dso_desc = DsoMap::find(dso_name);
    if (!dso_desc)
    {
        // Error - the plugin should have been found! If not then the plugin
        // likely does not have defined ShaderDescriptions matching 'plugin_name':
        std::cerr << "zpr::RayShader::ShaderDescription::find('" << dso_name << "') ";
        std::cerr << "error: plugin did not define a zpr::RayShader::ShaderDescription matching ";
        std::cerr << "the plugin name - this is likely a coding error.";
        if (dlerror())
            std::cerr << " '" << dlerror() << "'";
        std::cerr << std::endl;
        return NULL;  // plugin not found!
    }

    return dso_desc;
}


//-----------------------------------------------------------------------------


/*! Calc avoidance factor to compensate for the shadow-terminator problem.
    Adapted from the Lux project which implemented the paper
    "Taming the Shadow Terminator"
    https://www.yiningkarlli.com/projects/shadowterminator.html

    * Ninterpolated is the linearly-interpolated vertex normal
    * Nshading is the shading normal which may be bump-perturbed
    * Ldir is a direction normal pointing to the light

*/
/*static*/
float
RayShader::getShadowTerminatorAvoidanceFactor(const Fsr::Vec3d& Ninterpolated,
                                              const Fsr::Vec3d& Nshading,
                                              const Fsr::Vec3d& Ldir)
{
    const double Ns_dot_Ldir = Nshading.dot(Ldir);
    if (Ns_dot_Ldir <= 0.0)
        return 0.0;

    const double Ni_dot_Ns = Ninterpolated.dot(Nshading);
    if (Ni_dot_Ns <= 0.0)
        return 0.0;

    const double G = std::min(10.0, Ninterpolated.dot(Ldir) / (Ns_dot_Ldir * Ni_dot_Ns));
    if (G <= 0.0)
        return 0.0;

    const double G2 = G * G;
    const double G3 = G2 * G;

    return float(-G3 + G2 + G);
}


/*! Return the indirect diffuse illumination for surface point with normal N.
    Indirect diffuse means only rays that hit objects will contribute to the surface color.
*/
/*static*/
bool
RayShader::getIndirectDiffuse(RayShaderContext& stx,
                              const Fsr::Vec3d& N,
                              double            roughness,
                              Fsr::Pixel&       out)
{
    //std::cout << "getIndirectDiffuse(): depth=" << stx.diffuse_depth << " ray type=" << std::hex << stx.Rtx.type_mask << std::dec << std::endl;
    out.channels += DD::Image::Mask_RGBA;
    out.channels += stx.cutout_channel;
    out.clearAllChannels();

    // Check total & diffuse depth:
    if (stx.Rtx.isCameraPath())
        ++stx.diffuse_depth;
    if (stx.diffuse_depth >= stx.rtx->ray_diffuse_max_depth)
        return false;

    uint32_t hits = 0;
    const uint32_t nSamples = stx.sampler->diffuse_samples.size();
    for (uint32_t i=0; i < nSamples; ++i)
    {
        // Build a new direction vector oriented around N:
        const Sample2D& s = stx.sampler->diffuse_samples[i];
        Fsr::Vec3d Rd(s.dp.x*roughness,
                      s.dp.y*roughness,
                      1.0 - s.radius*roughness);
        Rd.normalize();
        Rd.orientAroundNormal(N, true/*auto_flip*/);
        if (Rd.dot(stx.Ng) < 0.0)
        {
            // Possibly skip rays that intersect plane of surface:
            if (i == nSamples-1 && hits == 0)
            {
                // No hits yet, do one last try that's not re-oriented:
                Rd = stx.Rtx.dir();
                if (Rd.dot(stx.Ng) < 0.0)
                    return false;
            }
            else
                continue; // skip if we have other rays to consider
        }

        // Build new diffuse ray:
        RayShaderContext stx_new(stx,
                                 Rd,
                                 std::numeric_limits<double>::epsilon(),
                                 std::numeric_limits<double>::infinity(),
                                 Fsr::RayContext::DIFFUSE | Fsr::RayContext::REFLECTION/*ray_type*/,
                                 RenderContext::SIDES_BOTH/*sides_mode*/);

        Fsr::Pixel illum(out.channels);
        RayMaterial::getIllumination(stx_new, illum, 0/*deep_out*/);
        if (illum[stx.cutout_channel] <= 0.5f)
        {
            out += illum;
            ++hits;
        }
    }

    if (hits == 0)
        return false;

    out /= float(nSamples);
    return true;
}


/*! Return the indirect specular illumination for surface point with normal N.
    Indirect specular means only reflected rays that hit objects will contribute to the surface color.
*/
/*static*/
bool
RayShader::getIndirectGlossy(RayShaderContext& stx,
                             const Fsr::Vec3d& N,
                             double            roughness,
                             Fsr::Pixel&       out)
{
    //std::cout << "  getIndirectGlossy(): glossy depth=" << stx.glossy_depth << " ray type=0x" << std::hex << stx.Rtx.type_mask << std::dec << " max glossy=" << stx.rtx->ray_glossy_max_depth << std::endl;
    out.channels += DD::Image::Mask_RGBA;
    out.channels += stx.cutout_channel;
    out.clearAllChannels();

    // Check total & glossy depth:
    if (stx.Rtx.isGlossyContributor())
        ++stx.glossy_depth;
    if (stx.glossy_depth >= stx.rtx->ray_glossy_max_depth)
        return false;

    // Reflect the view vector:
    Fsr::Vec3d V = stx.getViewVector(); // this may build a fake-stereo view-vector
    Fsr::Vec3d Rd_reflect = V.reflect(N);
    Rd_reflect.normalize();

    uint32_t hits = 0;
    const uint32_t nSamples = stx.sampler->glossy_samples.size();
    for (uint32_t i=0; i < nSamples; ++i)
    {
        // Build a new direction vector oriented around N:
        const Sample2D& s = stx.sampler->diffuse_samples[i];
        Fsr::Vec3d Rd(s.dp.x*roughness,
                      s.dp.y*roughness,
                      1.0 - s.radius*roughness);
        Rd.normalize();
        Rd.orientAroundNormal(Rd_reflect, true/*auto_flip*/);

        // Does the reflected ray intersect the plane of surface?:
        if (Rd.dot(stx.Ng) < 0.0)
        {
            // Yes, so reflect the ray *again*, this time using Ng,
            // which is the equivalent of
            // placing a parallel plane underneath this surface to 'catch'
            // the reflected ray and send it back 'up':
            const Fsr::Vec3d Vt = -Rd;
            Rd = Vt.reflect(stx.Ng);
            // If it's still a no go and we have no other hits, and this
            // is the last sample, give up (this shouldn't happen...):
            if (hits == 0 && i == nSamples-1 && Rd.dot(stx.Ng) < 0.0)
                return false;
        }

        // Build new glossy ray:
        RayShaderContext stx_new(stx,
                                 Rd,
                                 std::numeric_limits<double>::epsilon(),
                                 std::numeric_limits<double>::infinity(),
                                 Fsr::RayContext::GLOSSY | Fsr::RayContext::REFLECTION/*ray_type*/,
                                 RenderContext::SIDES_BOTH/*sides_mode*/);

        Fsr::Pixel illum(out.channels);
        RayMaterial::getIllumination(stx_new, illum, 0/*deep_out*/);
        if (illum[stx.cutout_channel] <= 0.5f)
        {
            out += illum;
            ++hits;
        }
    }
    if (hits == 0)
        return false;

    out /= float(nSamples);
    return true;
}


/*! Return the transmitted illumination for surface point with normal N.
    Transmission means only refracted rays that pass through objects will contribute to the surface color.
*/
/*static*/
bool
RayShader::getTransmission(RayShaderContext& stx,
                           const Fsr::Vec3d& N,
                           double            eta,
                           double            roughness,
                           Fsr::Pixel&       out)
{
    //std::cout << "getTransmission(): refraction_depth=" << stx.refraction_depth << " ray type=" << stx.Rtx.type_mask << " max refraction=" << stx.rtx->ray_refraction_max_depth << std::endl;
    out.channels += DD::Image::Mask_RGBA;
    out.channels += stx.cutout_channel;
    out.clearAllChannels();

    // Check total & glossy depth:
    if (stx.Rtx.isGlossyContributor())
        ++stx.refraction_depth;
    if (stx.refraction_depth >= stx.rtx->ray_refraction_max_depth)
        return false;

    // Refract the direction vector:
    Fsr::Vec3d Rd_refract(stx.Rtx.dir());
    RayShader::refract(stx.Rtx.dir(), stx.Nf, eta, Rd_refract);

    uint32_t hits = 0;
    const uint32_t nSamples = stx.sampler->refraction_samples.size();
    for (uint32_t i=0; i < nSamples; ++i)
    {
        // Build a new direction vector oriented around N:
        const Sample2D& s = stx.sampler->diffuse_samples[i];
        Fsr::Vec3d Rd(s.dp.x*roughness,
                      s.dp.y*roughness,
                      1.0 - s.radius*roughness);
        Rd.normalize();
        Rd.orientAroundNormal(Rd_refract, true/*auto_flip*/);
        if (Rd.dot(stx.Ng) >= 0.0)
        {
            // Possibly skip rays that intersect plane of surface:
            if (i == nSamples-1 && hits == 0)
            {
                // No hits yet, do one last try that's not re-oriented:
                Rd = stx.Rtx.dir();
                if (Rd.dot(stx.Ng) >= 0.0)
                    return false;
            }
            else
            {
                // Skip if we have other rays to consider:
                continue;
            }
        }
        //
        // Build new glossy ray:
        RayShaderContext stx_new(stx,
                                 Rd,
                                 std::numeric_limits<double>::epsilon(),
                                 std::numeric_limits<double>::infinity(),
                                 Fsr::RayContext::GLOSSY | Fsr::RayContext::REFLECTION/*ray_type*/,
                                 RenderContext::SIDES_BOTH/*sides_mode*/);

        Fsr::Pixel illum(out.channels);
        RayMaterial::getIllumination(stx_new, illum, 0/*deep_out*/);
        if (illum[stx.cutout_channel] <= 0.5f)
        {
            out += illum;
            ++hits;
        }
    }
    if (hits == 0)
        return false;

    out /= float(nSamples);
    return true;
}


/*! Get the occlusion of this surface point.

    For ambient occlusion set 'occlusion_ray_type' to DIFFUSE and
    for reflection occlusion use GLOSSY or REFLECTION, and
    TRANSMISSION for refraction occlusion.

    The value returned is between 0.0 and 1.0, where 0.0 means no
    occlusion (ie the point is completely exposed to the environment)
    and 1.0 is full-occlusion where the point has no exposure to the
    environment.
*/
/*static*/
float
RayShader::getOcclusion(RayShaderContext& stx,
                        uint32_t          occlusion_ray_type,
                        double            mindist,
                        double            maxdist,
                        double            cone_angle,
                        double            gi_scale)
{
    const SampleGrid2D* samples = NULL;

    Fsr::Vec3d N;
    switch (occlusion_ray_type)
    {
    default:
    case Fsr::RayContext::DIFFUSE:
        samples = &stx.sampler->diffuse_samples;
        N = stx.N;
        break;
    case Fsr::RayContext::REFLECTION:
    case Fsr::RayContext::GLOSSY:
    {
        samples = &stx.sampler->glossy_samples;
        Fsr::Vec3d V(-stx.Rtx.dir());
        N = V.reflect(stx.N);
        N.normalize();
        break;
    }
    case Fsr::RayContext::TRANSMISSION:
        samples = &stx.sampler->refraction_samples;
        N = -stx.N;
        break;
    case Fsr::RayContext::CAMERA:
        // Camera ray not supported for occlusion gathering:
        std::cerr << "RayShader::getOcclusion(): warning, camera ray type not supported." << std::endl;
        return 0.0f; // no occlusion
    case Fsr::RayContext::SHADOW:
        // Shadow ray not supported for occlusion gathering:
        std::cerr << "RayShader::getOcclusion(): warning, shadow ray type not supported." << std::endl;
        return 0.0f; // no occlusion
    }
    if (!samples)
        return 0.0f; // no occlusion

    if (::fabs(cone_angle) > 180.0)
        cone_angle = 180.0;
    const double cone_scale = (::fabs(cone_angle) / 180.0);

    float weight = 0.0f;
    const uint32_t nSamples = (cone_scale > std::numeric_limits<double>::epsilon()) ? samples->size() : 1;
    //std::cout << "getOcclusion [" << stx.x << " " << stx.y << "] N" << N << ", Ng" << stx.Ng;
    //std::cout << " samples=" << nSamples;
    //std::cout << std::endl;
    for (uint32_t i=0; i < nSamples; ++i)
    {
        const Sample2D& s = (*samples)[i];

        // Build a new direction vector from intersection normal:
        Fsr::Vec3d Rd(s.dp.x*cone_scale,
                      s.dp.y*cone_scale,
                      1.0 - s.radius*cone_scale);  // new ray direction
        Rd.normalize();
        Rd.orientAroundNormal(N, true/*auto_flip*/);
        if (Rd.dot(stx.Ng) < 0.0)
            continue; // skip sample rays that self-intersect

        // Build new occlusion ray:
        RayShaderContext stx_new(stx,
                                 Rd,
                                 mindist,
                                 maxdist,
                                 Fsr::RayContext::DIFFUSE | Fsr::RayContext::REFLECTION/*ray_type*/,
                                 RenderContext::SIDES_BOTH/*sides_mode*/);

        Traceable::SurfaceIntersection Iocl(std::numeric_limits<double>::infinity());
        if (stx.rtx->objects_bvh.getFirstIntersection(stx_new, Iocl) > Fsr::RAY_INTERSECT_NONE)
        {
            // Diffuse occlusion reduces the visibility weight by the hit distance:
            float vis = (occlusion_ray_type == Fsr::RayContext::DIFFUSE) ?
                            float(1.0 / ((Iocl.t*::fabs(gi_scale)) + 1.0)) : 1.0f;

            if (Iocl.object)
            {
                zpr::RenderPrimitive* rprim = static_cast<zpr::RenderPrimitive*>(Iocl.object);

                // Only check visibility if the rprim's material is a RayMaterial:
                if (rprim->surface_ctx->raymaterial)
                {
                    switch (occlusion_ray_type)
                    {
                    default:
                    case Fsr::RayContext::DIFFUSE:
                        if (!rprim->surface_ctx->raymaterial->getDiffuseVisibility())
                            vis = 0.0f;
                        break;
                    case Fsr::RayContext::REFLECTION:
                    case Fsr::RayContext::GLOSSY:
                        if (!rprim->surface_ctx->raymaterial->getSpecularVisibility())
                            vis = 0.0f;
                        break;
                    case Fsr::RayContext::TRANSMISSION:
                        if (!rprim->surface_ctx->raymaterial->getTransmissionVisibility())
                            vis = 0.0f;
                        break;
                    }

                }
            }

            weight += vis;
        }

    }

    if (weight <= 0.0f)
        return 0.0f; // no occlusion

    return clamp(weight / float(nSamples)); // partially exposed
}


} // namespace zpr

// end of zprender/RayShader.cpp

//
// Copyright 2020 DreamWorks Animation
//
