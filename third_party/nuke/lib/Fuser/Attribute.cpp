//
// Copyright 2019 DreamWorks Animation
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

/// @file Fuser/Attribute.cpp
///
/// @author Jonathan Egstad

#include "Attribute.h"
#include "AttributeTypes.h"

#include <map>
#include <mutex> // for std::mutex

namespace {


struct str_compare : std::binary_function<const char*, const char*, bool>
{
    bool operator () (const char* a, const char* b) const { return strcmp(a, b) < 0; }
};


typedef Fsr::Attribute* (*builder)();
typedef std::map<const char*, builder, str_compare> TypeMap;

static TypeMap    m_type_map;
static std::mutex m_map_lock;

}


namespace Fsr {


/*!
*/
/*static*/ bool
Attribute::haveType(const char* type)
{
    std::lock_guard<std::mutex> guard(m_map_lock);
    return (m_type_map.find(type) != m_type_map.end());
}


/*!
*/
/*static*/ void
Attribute::_addNewType(const char* type,
                       Attribute*  (*create)())
{
    std::lock_guard<std::mutex> guard(m_map_lock);
    if (m_type_map.find(type) == m_type_map.end())
        m_type_map.insert(TypeMap::value_type(type, create));
}


/*!
*/
/*static*/ Attribute*
Attribute::create(const char* type)
{
    std::lock_guard<std::mutex> guard(m_map_lock);
    const TypeMap::const_iterator it = m_type_map.find(type);
    return (it != m_type_map.end()) ? (it->second)() : NULL;
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

template <> /*static*/ const char*   BoolAttribute::_type()            { return "bool";   }
template <> /*static*/ const char*   BoolAttribute::_baseType()        { return "bool";   }
template <> /*static*/ uint32_t      BoolAttribute::_baseSize()        { return 1;        }
template <> /*static*/ uint32_t      BoolAttribute::_numBaseElements() { return 1;        }
template <> /*static*/ bool          BoolAttribute::_isArray()         { return false;    }
template <> /*static*/ const char*    IntAttribute::_type()            { return "int";    }
template <> /*static*/ const char*    IntAttribute::_baseType()        { return "int";    }
template <> /*static*/ uint32_t       IntAttribute::_baseSize()        { return 4;        }
template <> /*static*/ uint32_t       IntAttribute::_numBaseElements() { return 1;        }
template <> /*static*/ bool           IntAttribute::_isArray()         { return false;    }
template <> /*static*/ const char*  FloatAttribute::_type()            { return "float";  }
template <> /*static*/ const char*  FloatAttribute::_baseType()        { return "float";  }
template <> /*static*/ uint32_t     FloatAttribute::_baseSize()        { return 4;        }
template <> /*static*/ uint32_t     FloatAttribute::_numBaseElements() { return 1;        }
template <> /*static*/ bool         FloatAttribute::_isArray()         { return false;    }
template <> /*static*/ const char* DoubleAttribute::_type()            { return "double"; }
template <> /*static*/ const char* DoubleAttribute::_baseType()        { return "double"; }
template <> /*static*/ uint32_t    DoubleAttribute::_baseSize()        { return 8;        }
template <> /*static*/ uint32_t    DoubleAttribute::_numBaseElements() { return 1;        }
template <> /*static*/ bool        DoubleAttribute::_isArray()         { return false;    }
template <> /*static*/ const char* StringAttribute::_type()            { return "string"; }
template <> /*static*/ const char* StringAttribute::_baseType()        { return "string"; }
template <> /*static*/ uint32_t    StringAttribute::_baseSize()        { return 0;        } // non-POD
template <> /*static*/ uint32_t    StringAttribute::_numBaseElements() { return 1;        }
template <> /*static*/ bool        StringAttribute::_isArray()         { return false;    }
//
template <> /*static*/ const char*  Vec2fAttribute::_type()            { return "vec2f";  }
template <> /*static*/ const char*  Vec2fAttribute::_baseType()        { return "vec2f";  }
template <> /*static*/ uint32_t     Vec2fAttribute::_baseSize()        { return 4;        }
template <> /*static*/ uint32_t     Vec2fAttribute::_numBaseElements() { return 2;        }
template <> /*static*/ bool         Vec2fAttribute::_isArray()         { return false;    }
template <> /*static*/ const char*  Vec2dAttribute::_type()            { return "vec2d";  }
template <> /*static*/ const char*  Vec2dAttribute::_baseType()        { return "vec2d";  }
template <> /*static*/ uint32_t     Vec2dAttribute::_baseSize()        { return 8;        }
template <> /*static*/ uint32_t     Vec2dAttribute::_numBaseElements() { return 2;        }
template <> /*static*/ bool         Vec2dAttribute::_isArray()         { return false;    }
template <> /*static*/ const char*  Vec2iAttribute::_type()            { return "vec2i";  }
template <> /*static*/ const char*  Vec2iAttribute::_baseType()        { return "vec2i";  }
template <> /*static*/ uint32_t     Vec2iAttribute::_baseSize()        { return 4;        }
template <> /*static*/ uint32_t     Vec2iAttribute::_numBaseElements() { return 2;        }
template <> /*static*/ bool         Vec2iAttribute::_isArray()         { return false;    }
//
template <> /*static*/ const char*  Vec3fAttribute::_type()            { return "vec3f";  }
template <> /*static*/ const char*  Vec3fAttribute::_baseType()        { return "vec3f";  }
template <> /*static*/ uint32_t     Vec3fAttribute::_baseSize()        { return 4;        }
template <> /*static*/ uint32_t     Vec3fAttribute::_numBaseElements() { return 3;        }
template <> /*static*/ bool         Vec3fAttribute::_isArray()         { return false;    }
template <> /*static*/ const char*  Vec3dAttribute::_type()            { return "vec3d";  }
template <> /*static*/ const char*  Vec3dAttribute::_baseType()        { return "vec3d";  }
template <> /*static*/ uint32_t     Vec3dAttribute::_baseSize()        { return 8;        }
template <> /*static*/ uint32_t     Vec3dAttribute::_numBaseElements() { return 3;        }
template <> /*static*/ bool         Vec3dAttribute::_isArray()         { return false;    }
template <> /*static*/ const char*  Vec3iAttribute::_type()            { return "vec3i";  }
template <> /*static*/ const char*  Vec3iAttribute::_baseType()        { return "vec3i";  }
template <> /*static*/ uint32_t     Vec3iAttribute::_baseSize()        { return 4;        }
template <> /*static*/ uint32_t     Vec3iAttribute::_numBaseElements() { return 3;        }
template <> /*static*/ bool         Vec3iAttribute::_isArray()         { return false;    }
//
template <> /*static*/ const char*  Vec4fAttribute::_type()            { return "vec4f";  }
template <> /*static*/ const char*  Vec4fAttribute::_baseType()        { return "vec4f";  }
template <> /*static*/ uint32_t     Vec4fAttribute::_baseSize()        { return 4;        }
template <> /*static*/ uint32_t     Vec4fAttribute::_numBaseElements() { return 4;        }
template <> /*static*/ bool         Vec4fAttribute::_isArray()         { return false;    }
template <> /*static*/ const char*  Vec4dAttribute::_type()            { return "vec4d";  }
template <> /*static*/ const char*  Vec4dAttribute::_baseType()        { return "vec4d";  }
template <> /*static*/ uint32_t     Vec4dAttribute::_baseSize()        { return 8;        }
template <> /*static*/ uint32_t     Vec4dAttribute::_numBaseElements() { return 4;        }
template <> /*static*/ bool         Vec4dAttribute::_isArray()         { return false;    }
template <> /*static*/ const char*  Vec4iAttribute::_type()            { return "vec4i";  }
template <> /*static*/ const char*  Vec4iAttribute::_baseType()        { return "vec4i";  }
template <> /*static*/ uint32_t     Vec4iAttribute::_baseSize()        { return 4;        }
template <> /*static*/ uint32_t     Vec4iAttribute::_numBaseElements() { return 4;        }
template <> /*static*/ bool         Vec4iAttribute::_isArray()         { return false;    }
//
template <> /*static*/ const char*  Box2fAttribute::_type()            { return "box2f";  }
template <> /*static*/ const char*  Box2fAttribute::_baseType()        { return "box2f";  }
template <> /*static*/ uint32_t     Box2fAttribute::_baseSize()        { return 4;        }
template <> /*static*/ uint32_t     Box2fAttribute::_numBaseElements() { return 4;        }
template <> /*static*/ bool         Box2fAttribute::_isArray()         { return false;    }
template <> /*static*/ const char*  Box2dAttribute::_type()            { return "box2d";  }
template <> /*static*/ const char*  Box2dAttribute::_baseType()        { return "box2d";  }
template <> /*static*/ uint32_t     Box2dAttribute::_baseSize()        { return 8;        }
template <> /*static*/ uint32_t     Box2dAttribute::_numBaseElements() { return 4;        }
template <> /*static*/ bool         Box2dAttribute::_isArray()         { return false;    }
template <> /*static*/ const char*  Box2iAttribute::_type()            { return "box2i";  }
template <> /*static*/ const char*  Box2iAttribute::_baseType()        { return "box2i";  }
template <> /*static*/ uint32_t     Box2iAttribute::_baseSize()        { return 4;        }
template <> /*static*/ uint32_t     Box2iAttribute::_numBaseElements() { return 4;        }
template <> /*static*/ bool         Box2iAttribute::_isArray()         { return false;    }
//
template <> /*static*/ const char*  Box3fAttribute::_type()            { return "box3f";  }
template <> /*static*/ const char*  Box3fAttribute::_baseType()        { return "box3f";  }
template <> /*static*/ uint32_t     Box3fAttribute::_baseSize()        { return 4;        }
template <> /*static*/ uint32_t     Box3fAttribute::_numBaseElements() { return 6;        }
template <> /*static*/ bool         Box3fAttribute::_isArray()         { return false;    }
template <> /*static*/ const char*  Box3dAttribute::_type()            { return "box3d";  }
template <> /*static*/ const char*  Box3dAttribute::_baseType()        { return "box3d";  }
template <> /*static*/ uint32_t     Box3dAttribute::_baseSize()        { return 8;        }
template <> /*static*/ uint32_t     Box3dAttribute::_numBaseElements() { return 6;        }
template <> /*static*/ bool         Box3dAttribute::_isArray()         { return false;    }
template <> /*static*/ const char*  Box3iAttribute::_type()            { return "box3i";  }
template <> /*static*/ const char*  Box3iAttribute::_baseType()        { return "box3i";  }
template <> /*static*/ uint32_t     Box3iAttribute::_baseSize()        { return 4;        }
template <> /*static*/ uint32_t     Box3iAttribute::_numBaseElements() { return 6;        }
template <> /*static*/ bool         Box3iAttribute::_isArray()         { return false;    }
//
template <> /*static*/ const char*  Mat4fAttribute::_type()            { return "mat4f";  }
template <> /*static*/ const char*  Mat4fAttribute::_baseType()        { return "mat4f";  }
template <> /*static*/ uint32_t     Mat4fAttribute::_baseSize()        { return 4;        }
template <> /*static*/ uint32_t     Mat4fAttribute::_numBaseElements() { return 16;       }
template <> /*static*/ bool         Mat4fAttribute::_isArray()         { return false;    }
template <> /*static*/ const char*  Mat4dAttribute::_type()            { return "mat4d";  }
template <> /*static*/ const char*  Mat4dAttribute::_baseType()        { return "mat4d";  }
template <> /*static*/ uint32_t     Mat4dAttribute::_baseSize()        { return 8;        }
template <> /*static*/ uint32_t     Mat4dAttribute::_numBaseElements() { return 16;       }
template <> /*static*/ bool         Mat4dAttribute::_isArray()         { return false;    }

//-------------------------------------------------------------------------

template <> /*static*/ const char*   BoolListAttribute::_type()            { return "boollist";  }
template <> /*static*/ const char*   BoolListAttribute::_baseType()        { return "bool";      }
template <> /*static*/ uint32_t      BoolListAttribute::_baseSize()        { return 1;           }
template <> /*static*/ uint32_t      BoolListAttribute::_numBaseElements() { return 1;           }
template <> /*static*/ bool          BoolListAttribute::_isArray()         { return true;        }
template <> /*static*/ const char*    IntListAttribute::_type()            { return "intlist";   }
template <> /*static*/ const char*    IntListAttribute::_baseType()        { return "int";       }
template <> /*static*/ uint32_t       IntListAttribute::_baseSize()        { return 4;           }
template <> /*static*/ uint32_t       IntListAttribute::_numBaseElements() { return 1;           }
template <> /*static*/ bool           IntListAttribute::_isArray()         { return true;        }
template <> /*static*/ const char*  FloatListAttribute::_type()            { return "floatlist"; }
template <> /*static*/ const char*  FloatListAttribute::_baseType()        { return "float";     }
template <> /*static*/ uint32_t     FloatListAttribute::_baseSize()        { return 4;           }
template <> /*static*/ uint32_t     FloatListAttribute::_numBaseElements() { return 1;           }
template <> /*static*/ bool         FloatListAttribute::_isArray()         { return true;        }
template <> /*static*/ const char* DoubleListAttribute::_type()            { return "doublelist";}
template <> /*static*/ const char* DoubleListAttribute::_baseType()        { return "double";    }
template <> /*static*/ uint32_t    DoubleListAttribute::_baseSize()        { return 8;           }
template <> /*static*/ uint32_t    DoubleListAttribute::_numBaseElements() { return 1;           }
template <> /*static*/ bool        DoubleListAttribute::_isArray()         { return true;        }
template <> /*static*/ const char* StringListAttribute::_type()            { return "stringlist";}
template <> /*static*/ const char* StringListAttribute::_baseType()        { return "string";    }
template <> /*static*/ uint32_t    StringListAttribute::_baseSize()        { return 0;           } // non-POD
template <> /*static*/ uint32_t    StringListAttribute::_numBaseElements() { return 1;           }
template <> /*static*/ bool        StringListAttribute::_isArray()         { return true;        }
//
template <> /*static*/ const char*  Vec2fListAttribute::_type()            { return "vec2flist"; }
template <> /*static*/ const char*  Vec2fListAttribute::_baseType()        { return "vec2f";     }
template <> /*static*/ uint32_t     Vec2fListAttribute::_baseSize()        { return 4;           }
template <> /*static*/ uint32_t     Vec2fListAttribute::_numBaseElements() { return 2;           }
template <> /*static*/ bool         Vec2fListAttribute::_isArray()         { return true;        }
template <> /*static*/ const char*  Vec2dListAttribute::_type()            { return "vec2dlist"; }
template <> /*static*/ const char*  Vec2dListAttribute::_baseType()        { return "vec2d";     }
template <> /*static*/ uint32_t     Vec2dListAttribute::_baseSize()        { return 8;           }
template <> /*static*/ uint32_t     Vec2dListAttribute::_numBaseElements() { return 2;           }
template <> /*static*/ bool         Vec2dListAttribute::_isArray()         { return true;        }
template <> /*static*/ const char*  Vec2iListAttribute::_type()            { return "vec2ilist"; }
template <> /*static*/ const char*  Vec2iListAttribute::_baseType()        { return "vec2i";     }
template <> /*static*/ uint32_t     Vec2iListAttribute::_baseSize()        { return 4;           }
template <> /*static*/ uint32_t     Vec2iListAttribute::_numBaseElements() { return 2;           }
template <> /*static*/ bool         Vec2iListAttribute::_isArray()         { return true;        }
//
template <> /*static*/ const char*  Vec3fListAttribute::_type()            { return "vec3flist"; }
template <> /*static*/ const char*  Vec3fListAttribute::_baseType()        { return "vec3f";     }
template <> /*static*/ uint32_t     Vec3fListAttribute::_baseSize()        { return 4;           }
template <> /*static*/ uint32_t     Vec3fListAttribute::_numBaseElements() { return 3;           }
template <> /*static*/ bool         Vec3fListAttribute::_isArray()         { return true;        }
template <> /*static*/ const char*  Vec3dListAttribute::_type()            { return "vec3dlist"; }
template <> /*static*/ const char*  Vec3dListAttribute::_baseType()        { return "vec3d";     }
template <> /*static*/ uint32_t     Vec3dListAttribute::_baseSize()        { return 8;           }
template <> /*static*/ uint32_t     Vec3dListAttribute::_numBaseElements() { return 3;           }
template <> /*static*/ bool         Vec3dListAttribute::_isArray()         { return true;        }
template <> /*static*/ const char*  Vec3iListAttribute::_type()            { return "vec3ilist"; }
template <> /*static*/ const char*  Vec3iListAttribute::_baseType()        { return "vec3i";     }
template <> /*static*/ uint32_t     Vec3iListAttribute::_baseSize()        { return 4;           }
template <> /*static*/ uint32_t     Vec3iListAttribute::_numBaseElements() { return 3;           }
template <> /*static*/ bool         Vec3iListAttribute::_isArray()         { return true;        }
//
template <> /*static*/ const char*  Vec4fListAttribute::_type()            { return "vec4flist"; }
template <> /*static*/ const char*  Vec4fListAttribute::_baseType()        { return "vec4f";     }
template <> /*static*/ uint32_t     Vec4fListAttribute::_baseSize()        { return 4;           }
template <> /*static*/ uint32_t     Vec4fListAttribute::_numBaseElements() { return 4;           }
template <> /*static*/ bool         Vec4fListAttribute::_isArray()         { return true;        }
template <> /*static*/ const char*  Vec4dListAttribute::_type()            { return "vec4dlist"; }
template <> /*static*/ const char*  Vec4dListAttribute::_baseType()        { return "vec4d";     }
template <> /*static*/ uint32_t     Vec4dListAttribute::_baseSize()        { return 8;           }
template <> /*static*/ uint32_t     Vec4dListAttribute::_numBaseElements() { return 4;           }
template <> /*static*/ bool         Vec4dListAttribute::_isArray()         { return true;        }
template <> /*static*/ const char*  Vec4iListAttribute::_type()            { return "vec4ilist"; }
template <> /*static*/ const char*  Vec4iListAttribute::_baseType()        { return "vec4i";     }
template <> /*static*/ uint32_t     Vec4iListAttribute::_baseSize()        { return 4;           }
template <> /*static*/ uint32_t     Vec4iListAttribute::_numBaseElements() { return 4;           }
template <> /*static*/ bool         Vec4iListAttribute::_isArray()         { return true;        }
//
template <> /*static*/ const char*  Box2fListAttribute::_type()            { return "box2flist"; }
template <> /*static*/ const char*  Box2fListAttribute::_baseType()        { return "box2f";     }
template <> /*static*/ uint32_t     Box2fListAttribute::_baseSize()        { return 4;           }
template <> /*static*/ uint32_t     Box2fListAttribute::_numBaseElements() { return 4;           }
template <> /*static*/ bool         Box2fListAttribute::_isArray()         { return true;        }
template <> /*static*/ const char*  Box2dListAttribute::_type()            { return "box2dlist"; }
template <> /*static*/ const char*  Box2dListAttribute::_baseType()        { return "box2d";     }
template <> /*static*/ uint32_t     Box2dListAttribute::_baseSize()        { return 8;           }
template <> /*static*/ uint32_t     Box2dListAttribute::_numBaseElements() { return 4;           }
template <> /*static*/ bool         Box2dListAttribute::_isArray()         { return true;        }
template <> /*static*/ const char*  Box2iListAttribute::_type()            { return "box2ilist"; }
template <> /*static*/ const char*  Box2iListAttribute::_baseType()        { return "box2i";     }
template <> /*static*/ uint32_t     Box2iListAttribute::_baseSize()        { return 4;           }
template <> /*static*/ uint32_t     Box2iListAttribute::_numBaseElements() { return 4;           }
template <> /*static*/ bool         Box2iListAttribute::_isArray()         { return true;        }
//
template <> /*static*/ const char*  Box3fListAttribute::_type()            { return "box3flist"; }
template <> /*static*/ const char*  Box3fListAttribute::_baseType()        { return "box3f";     }
template <> /*static*/ uint32_t     Box3fListAttribute::_baseSize()        { return 4;           }
template <> /*static*/ uint32_t     Box3fListAttribute::_numBaseElements() { return 6;           }
template <> /*static*/ bool         Box3fListAttribute::_isArray()         { return true;        }
template <> /*static*/ const char*  Box3dListAttribute::_type()            { return "box3dlist"; }
template <> /*static*/ const char*  Box3dListAttribute::_baseType()        { return "box3d";     }
template <> /*static*/ uint32_t     Box3dListAttribute::_baseSize()        { return 8;           }
template <> /*static*/ uint32_t     Box3dListAttribute::_numBaseElements() { return 6;           }
template <> /*static*/ bool         Box3dListAttribute::_isArray()         { return true;        }
template <> /*static*/ const char*  Box3iListAttribute::_type()            { return "box3ilist"; }
template <> /*static*/ const char*  Box3iListAttribute::_baseType()        { return "box3i";     }
template <> /*static*/ uint32_t     Box3iListAttribute::_baseSize()        { return 4;           }
template <> /*static*/ uint32_t     Box3iListAttribute::_numBaseElements() { return 6;           }
template <> /*static*/ bool         Box3iListAttribute::_isArray()         { return true;        }
//
template <> /*static*/ const char*  Mat4fListAttribute::_type()            { return "mat4flist"; }
template <> /*static*/ const char*  Mat4fListAttribute::_baseType()        { return "mat4f";     }
template <> /*static*/ uint32_t     Mat4fListAttribute::_baseSize()        { return 4;           }
template <> /*static*/ uint32_t     Mat4fListAttribute::_numBaseElements() { return 16;          }
template <> /*static*/ bool         Mat4fListAttribute::_isArray()         { return true;        }
template <> /*static*/ const char*  Mat4dListAttribute::_type()            { return "mat4dlist"; }
template <> /*static*/ const char*  Mat4dListAttribute::_baseType()        { return "mat4d";     }
template <> /*static*/ uint32_t     Mat4dListAttribute::_baseSize()        { return 8;           }
template <> /*static*/ uint32_t     Mat4dListAttribute::_numBaseElements() { return 16;          }
template <> /*static*/ bool         Mat4dListAttribute::_isArray()         { return true;        }

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


} // namespace Fsr


// end of Fuser/Attribute.cpp

//
// Copyright 2019 DreamWorks Animation
//
