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

/// @file zprender/Bvh.h
///
/// @author Jonathan Egstad


#ifndef zprender_Bvh_h
#define zprender_Bvh_h

#include "Traceable.h"

#include <Fuser/Box3.h>
#include <Fuser/AttributeTypes.h> // for Vec3fList

namespace zpr {

// zpr::Bvh enumerations used for SurfaceIntersection::object_type:
static const uint32_t ZprBvh = 11;


// Uncomment to get a bunch of debug prints during Bvh node build
//#define DEBUG_BVH_BUILD 1

//! Used in Bvh and other places that return a const Fsr::Box3<T>&
extern FSR_EXPORT Fsr::Box3f empty_box3f;
extern FSR_EXPORT Fsr::Box3d empty_box3d;


//---------------------------------------------------------------------------------


/*! Temp structure to speed up building of hierarchy.
*/
template <class T>
struct BvhObjRef
{
    T          data;    //!< Usually an index or a pointer
    Fsr::Box3f bbox;    //!< 

    //! Default ctor leaves junk in vars.
    BvhObjRef() {}
    //!
    BvhObjRef(T _data, const Fsr::Box3f& _bbox) : data(_data), bbox(_bbox) {}
};


//---------------------------------------------------------------------------------


/*! This comes from PBR example on how to flatten a bvh.
*/
struct ZPR_EXPORT BvhNode
{
    Fsr::Box3f bbox;            //!< AABB bbox of node
    union
    {
        uint32_t items_start;   //!< Leaf node, start of items in Bvh items list.
        uint32_t B_offset;      //!< Interior node, offset to B node
    };
    uint16_t   num_items;       //!< 1+ == a leaf node
    uint8_t    split_axis;      //!< Non-leaf node split direction (0=x, 1=y, 2=z)
    uint8_t    depth;           //!< Depth level (and to ensure 32 byte total size for mem alignment)

    //!
    bool     isLeaf()    const { return (num_items > 0); }
    //!
    uint32_t itemStart() const { return items_start; }
    //!
    uint32_t numItems()  const { return (uint32_t)num_items; }
    //!
    uint32_t getDepth()  const { return (uint32_t)depth; }
};

typedef std::vector<BvhNode> BvhNodeList;


//---------------------------------------------------------------------------------


/*!
*/
template <class T>
class ZPR_EXPORT Bvh : public Traceable
{
  protected:
    /*! Temp node used during Bvh construction.
        These are converted to BvhNodes in _flatten() method.
    */
    struct BuilderNode
    {
        Fsr::Box3f  bbox;       //!< AABB bbox of node
        BuilderNode *A, *B;     //!< Child nodes
        uint32_t    start, end; //!< Range of data chunks this node contains inside bbox
        uint8_t     split_axis; //!< Node split direction (0=x, 1=y, 2=z)
        uint8_t     depth;      //!< Depth level
        uint8_t     pad[30];    //!< Ensure 128 byte total size for mem alignment (is this really necessary?)

        //!
        bool     isLeaf()   const { return (A==NULL && B==NULL); }
        //!
        uint32_t numItems() const { return (end - start); }

        BuilderNode() : A(NULL), B(NULL) {}
        ~BuilderNode() { delete A; delete B; }
    };


  protected:
    std::string    m_name;          //!< Identifier string, usually for debugging
    std::vector<T> m_item_list;     //!< List of all data items in Bvh
    BvhNodeList    m_node_list;     //!< List of flattened BvhNodes
    uint32_t       m_max_objects;   //!< Max number of objects in a leaf node
    uint32_t       m_max_depth;     //!< Depth of lowest leaf in Bvh
    Fsr::Vec3d     m_bbox_origin;   //!< Global offset for bboxes used during intersection tests


  public:
    //!
    Bvh();

    //! If this is a traceable primitive return this cast to Traceable.
    /*virtual*/ Traceable* isTraceable() { return this; }

    //!
    const Fsr::Box3f& bbox()    const { return (m_node_list.size() > 0) ? m_node_list[0].bbox : empty_box3f; }
    bool              isEmpty() const { return ((m_node_list.size() > 0) ? m_node_list[0].bbox.isEmpty() : true); }

    //! Global origin offset applied to the bboxes.
    void              setGlobalOrigin(const Fsr::Vec3d& P) { m_bbox_origin = P; }
    const Fsr::Vec3d& getGlobalOrigin() const { return m_bbox_origin; }


    //! Node access.
    uint32_t           maxNodeDepth()      const { return m_max_depth; }
    uint32_t           numNodes()          const { return (uint32_t)m_node_list.size(); }
    const BvhNodeList& nodeList()          const { return m_node_list; }
    const BvhNode&     getNode(uint32_t i) const { return m_node_list[i]; }

    //! Items access.
    uint32_t              numItems()          const { return (uint32_t)m_item_list.size(); }
    const std::vector<T>& itemList()          const { return m_item_list; }
    const T&              getItem(uint32_t i) const { return m_item_list[i]; }

    //! Name string is mainly for debugging.
    void        setName(const char* name) { if (name && name[0]) m_name = name; }
    const char* getName() const { return m_name.c_str(); }


    //---------------------------------------------------------------------


    //! Empty the Bvh and make ready for a new build.
    void clear();

    /*! Recursively build the hierarchy.
        Note - this will reorder the 'obj_refs' array contents so
        any indices into it are only valid after build() is complete.
    */
    void build(std::vector<BvhObjRef<T> >& obj_refs,
               uint32_t                    max_objects_per_leaf=1);


    //!
    bool getIntersectedLeafs(Fsr::RayContext&             Rtx,
                             std::vector<const BvhNode*>& node_list) const;


    //--------------------------------------------------------------------------------- 
    // From Traceable:

    //! Intersect a ray with this object.  This doesn't return any additional info.
    /*virtual*/ bool intersect(RayShaderContext& stx);

    //! Intersect a ray with all surfaces of this object. Returns the type of intersection code.
    /*virtual*/ void getIntersections(RayShaderContext&        stx,
                                      SurfaceIntersectionList& I_list,
                                      double&                  tmin,
                                      double&                  tmax);

    //! Intersect a ray with this first surface of this object. Returns the type of intersection code.
    /*virtual*/ Fsr::RayIntersectionType getFirstIntersection(RayShaderContext&    stx,
                                                              SurfaceIntersection& I);

    //! Intersect against a specific depth level, usually for debugging.
    /*virtual*/ int intersectLevel(RayShaderContext& stx,
                                   int               level,
                                   int               max_level=1000000);

    //! Intersect a 2D line with the primitive's UV coords and return the intersection.
    /*virtual*/ void getIntersectionsWithUVs(RayShaderContext&          stx,
                                             const Fsr::Vec2f&          uv0,
                                             const Fsr::Vec2f&          uv1,
                                             UVSegmentIntersectionList& I_list);

    //--------------------------------------------------------------------------------- 


  protected:
    /*! Recursively build the hierarchy.
        Note - this will reorder the 'obj_refs' and 'obj_centers' array
        contents so indices into them are only valid after _build() is
        complete.
    */
    void _build(BuilderNode*                bvh_node,
                std::vector<BvhObjRef<T> >& obj_refs,
                Fsr::Vec3fList&             obj_centers,
                uint32_t                    start,
                uint32_t                    end,
                uint32_t&                   nNodes,
                uint32_t&                   max_depth);

    /*! Must preallocate an array of BvhNodes to the total number of nodes
        returned by the _build() method.
    */
    uint32_t _flatten(const BuilderNode* bvh_node,
                      uint32_t&          offset);


};



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/*                   Inline Function Implementations                   */
/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/


//!
template <class T>
Bvh<T>::Bvh() :
    m_max_objects(1),
    m_max_depth(0),
    m_bbox_origin(0,0,0)
{
    //
}



/*! Empty the Bvh and make ready for a new build.
*/
template <class T>
inline void
Bvh<T>::clear()
{
    m_name.clear();
    m_item_list   = std::vector<T>(); // releases allocation
    m_node_list   = BvhNodeList();    // releases allocation
    m_max_objects = 1;
    m_max_depth   = 0;
    m_bbox_origin.set(0,0,0);
}



/*! Recursively build the hierarchy.
    Note - this will reorder the 'obj_refs' array contents so
    any indices into it are only valid after build() is complete.
*/
template <class T>
inline void
Bvh<T>::build(std::vector<BvhObjRef<T> >& obj_refs,
              uint32_t                    max_objects_per_leaf)
{
#ifdef DEBUG_BVH_BUILD
    std::cout << "-------------------------------------------------" << std::endl;
    std::cout << "Bvh::build(" << this << ")";
#endif

    // Delete any existing hierarchy:
    if (m_node_list.size() > 0)
        clear();

    m_max_objects = max_objects_per_leaf;

    const uint32_t nObjRefs = (uint32_t)obj_refs.size();
    if (nObjRefs == 0)
    {
        // nothing to build!
#ifdef DEBUG_BVH_BUILD
        std::cout << " no refs, empty Bvh." << std::endl;
#endif
    }
    else
    {
        // ObjRef centers list is kept in sync with
        // ObjRef list:
        Fsr::Vec3fList obj_centers(nObjRefs);
        for (uint32_t i=0; i < nObjRefs; ++i)
            obj_centers[i] = obj_refs[i].bbox.getCenter();

#ifdef DEBUG_BVH_BUILD
        std::cout << "  builder nodes list:" << std::endl;
#endif
        uint32_t nNodes = 0;
        m_max_depth = 0;
        BuilderNode* root_node = new BuilderNode();
        this->_build(root_node,
                     obj_refs,
                     obj_centers,
                     0/*start*/,
                     nObjRefs/*end*/,
                     nNodes,
                     m_max_depth);

        if (nNodes > 0)
        {
            // Copy data from the reordered ObjRefList
            // to the data_chunks list, then flatten:
            m_item_list.resize(nObjRefs);
            for (uint32_t i=0; i < nObjRefs; ++i)
                m_item_list[i] = obj_refs[i].data;

#ifdef DEBUG_BVH_BUILD
            std::cout << "  flattened node list:" << std::endl;
#endif

            m_node_list.resize(nNodes);
            uint32_t offset = 0;
            this->_flatten(root_node, offset);
#if DEBUG
            assert(offset == m_node_list.size());
#endif
        }
        delete root_node;

#ifdef DEBUG_BVH_BUILD
        std::cout << "  numItems=" << numItems() << "  " << this->bbox();
        std::cout << ", nNodes=" << nNodes << ", m_max_depth=" << m_max_depth;
        std::cout << std::endl;
#endif
    }

#ifdef DEBUG_BVH_BUILD
    std::cout << "-------------------------------------------------" << std::endl;
#endif
}



/*! Recursively build the hierarchy.
    Note - this will reorder the 'obj_refs' and 'obj_centers' array
    contents so indices into them are only valid after _build() is
    complete.
*/
template <class T>
inline void
Bvh<T>::_build(BuilderNode*                bvh_node,
               std::vector<BvhObjRef<T> >& obj_refs,
               Fsr::Vec3fList&             obj_centers,
               uint32_t                    start,
               uint32_t                    end,
               uint32_t&                   nNodes,
               uint32_t&                   max_depth)
{
#if DEBUG
    assert(bvh_node);
    assert(obj_refs.size() == obj_centers.size());
#endif
    ++nNodes;

    bvh_node->start = start;
    bvh_node->end   = end;
    const int32_t nObjects = (end - start);
    if (nObjects <= 0)
        assert(nObjects > 0); // shouldn't happen!

    // Concatenate all the object bboxes and obj_centers:
    Fsr::Box3f bbox   = obj_refs[start].bbox;
    Fsr::Vec3f weight = obj_centers[start];
    for (uint32_t i=(start+1); i < end; ++i)
    {
        bbox.expand(obj_refs[i].bbox, false/*test_empty*/);
        weight += obj_centers[i];
    }
    bvh_node->bbox  = bbox;
    bvh_node->depth = std::min((uint8_t)max_depth, (uint8_t)255);
#ifdef DEBUG_BVH_BUILD
    std::cout << "    " << max_depth << ":" << nNodes << " " << nObjects << bbox;
#endif

    if (nObjects <= (int32_t)m_max_objects)
    {
        bvh_node->A = bvh_node->B = NULL; // mark node as leaf
#ifdef DEBUG_BVH_BUILD
        std::cout << " LEAF" << std::endl;
#endif
        return;
    }

    // Ok, more than two objects require us to clump them in groupings
    // that fall on each side of a split point, which is the center
    // of the whole bbox.  Other split points can be calculated but
    // that's what we're doing for the moment:
    weight /= float(nObjects);
#ifdef DEBUG_BVH_BUILD
    std::cout << " wt=" << weight;
#endif

    // Find the largest dimension, X, Y, or Z, then find the dividing point
    // to split the range of objects in two.  If an object's bbox intersects a
    // side then it gets moved to that side.  We choose the largest side
    // to balance out the splitting:
    const Fsr::Vec3f size(bvh_node->bbox.max - bvh_node->bbox.min);

    uint8_t split_axis = 0;
    if (size[1] > size[0])
        split_axis = 1; // Y-split
    if (size[2] > size[split_axis])
        split_axis = 2; // Z-split

    // TODO: enhance split point logic.  For now we just use the midpoint:
    const float split_point = weight[split_axis];//(m_bbox.min[m_split_axis] + m_bbox.max[m_split_axis]) / 2.0f;
#ifdef DEBUG_BVH_BUILD
    std::cout << "  split_axis=" << (int)split_axis << " dist=" << split_point << std::endl;
#endif

    // Re-order the object range into two new ranges, one for each side of the hierarchy:
    uint32_t mid = start;
    for (uint32_t i=start; i < end; ++i)
    {
        if (obj_centers[i][split_axis] < split_point)
        {
            std::swap(obj_refs[i],    obj_refs[mid]   );
            std::swap(obj_centers[i], obj_centers[mid]);
            ++mid;
        }
    }
    bvh_node->split_axis = split_axis;

    // If all the objects end up on one side then split the list down the middle:
    if (mid == start || mid == end)
        mid = (start + end)/2;

#ifdef DEBUG_BVH_BUILD
    std::cout << "      split list: ";
    for (uint32_t i=start; i < mid; ++i)
        std::cout << " " << i;
    std::cout << " ||";
    for (uint32_t i=mid; i < end; ++i)
        std::cout << " " << i;
    std::cout << std::endl;
#endif

    ++max_depth;
    uint32_t depthA = max_depth;
    uint32_t depthB = max_depth;
    if (start < mid)
    {
        bvh_node->A = new BuilderNode();
        _build(bvh_node->A, obj_refs, obj_centers, start, mid, nNodes, depthA);
    }
    if (mid < end)
    {
        bvh_node->B = new BuilderNode();
        _build(bvh_node->B, obj_refs, obj_centers, mid,   end, nNodes, depthB);
    }
    max_depth = std::max(depthA, depthB);
}


/*! Must preallocate an array of BvhNodes to the total number of nodes
    returned by the _build() method.
*/
template <class T>
inline uint32_t
Bvh<T>::_flatten(const BuilderNode* bvh_node,
                 uint32_t&          offset)
{
    const uint32_t node_index = offset++;
#if DEBUG
    assert(node_index < m_node_list.size());
#endif
    BvhNode& flat_node = m_node_list[node_index];

    flat_node.bbox  = bvh_node->bbox;
    flat_node.depth = bvh_node->depth;
    if (bvh_node->isLeaf())
    {
        flat_node.items_start = bvh_node->start;
        flat_node.num_items   = uint16_t(bvh_node->end - bvh_node->start);
    }
    else
    {
        flat_node.num_items  = 0;
        flat_node.split_axis = bvh_node->split_axis;

        // Flatten children:
        if (bvh_node->A)
            _flatten(bvh_node->A, offset);
        if (bvh_node->B)
            flat_node.B_offset = _flatten(bvh_node->B, offset);
    }

#ifdef DEBUG_BVH_BUILD
    std::cout << "    " << node_index << flat_node.bbox << ", depth=" << (int)flat_node.depth;
    if (bvh_node->isLeaf())
    {
        std::cout << ", leaf: items_start=" << flat_node.items_start;
        std::cout << ", numItems=" << flat_node.num_items;
    }
    else
    {
        std::cout << ", B_offset=" << flat_node.B_offset;
    }
    std::cout << std::endl;
#endif

    return node_index;
}


//--------------------------------------------------------------------------


/*! */
template <class T>
inline bool
Bvh<T>::getIntersectedLeafs(Fsr::RayContext&             Rtx,
                            std::vector<const BvhNode*>& node_list) const
{
    node_list.clear();
    if (this->isEmpty())
        return false;

    uint32_t current_node_index  = 0;
    uint32_t next_to_visit_index = 0;
    uint32_t nodes_to_visit_stack[256];
    while (1)
    {
        const BvhNode& node = m_node_list[current_node_index];
        if (Fsr::intersectAABB(node.bbox, m_bbox_origin, Rtx))
        {
            if (node.isLeaf())
            {
                node_list.push_back(&node);
                if (next_to_visit_index == 0)
                    break;
                --next_to_visit_index;
                current_node_index = nodes_to_visit_stack[next_to_visit_index];
            }
            else
            {
                // Put far Bvh node on nodes_to_visit_stack, advance to near node
                if (Rtx.isSlopePositive(node.split_axis))
                {
                    nodes_to_visit_stack[next_to_visit_index++] = node.B_offset;
                    current_node_index = (current_node_index + 1);
                }
                else
                {
                    nodes_to_visit_stack[next_to_visit_index++] = (current_node_index + 1);
                    current_node_index = node.B_offset;
                }
            }
        }
        else
        {
            if (next_to_visit_index == 0)
                break;
            --next_to_visit_index;
            current_node_index = nodes_to_visit_stack[next_to_visit_index];
        }

    }
    return (node_list.size() > 0);
}


//! Intersect a ray with this object.  This doesn't return any additional info.
/*virtual*/
template <class T>
inline bool
Bvh<T>::intersect(RayShaderContext& stx)
{
    if (m_node_list.size() == 0)
        return false;
    double tmin, tmax;
    return Fsr::intersectAABB(m_node_list[0].bbox, m_bbox_origin, stx.Rtx, tmin, tmax);
}


/*virtual*/
template <class T>
inline Fsr::RayIntersectionType
Bvh<T>::getFirstIntersection(RayShaderContext&    stx,
                             SurfaceIntersection& I)
{
    std::cout << "Bvh<T>::getFirstIntersection(" << this << ") not implemented!" << std::endl;
    return Fsr::RAY_INTERSECT_NONE;
}


/*virtual*/
template<class T>
inline void
Bvh<T>::getIntersections(RayShaderContext&        stx,
                         SurfaceIntersectionList& I_list,
                         double&                  tmin,
                         double&                  tmax)
{
    std::cout << "Bvh<T>::getIntersections(" << this << ") not implemented!" << std::endl;
}


/*virtual*/
template <class T>
inline int
Bvh<T>::intersectLevel(RayShaderContext& stx,
                       int               level,
                       int               max_level)
{
    //std::cout << "        Bvh<T>::intersectLevel(" << this << ")" << this->bbox();
    //std::cout << ": current-level=" << level << ", max_level=" << max_level << std::endl;
    if (this->isEmpty())
        return level;

    int out_level = level;
    uint32_t current_node_index  = 0;
    uint32_t next_to_visit_index = 0;
    uint32_t nodes_to_visit_stack[256];
    while (1)
    {
        const BvhNode& node = m_node_list[current_node_index];
        //std::cout << "    " << current_node_index << " node" << node.bbox << ", depth=" << node.getDepth() << std::endl;
        if (Fsr::intersectAABB(node.bbox, m_bbox_origin, stx.Rtx))
        {
            const int node_level = level + 1 + node.getDepth();
            if (node_level >= max_level)
                return node_level; // at max level

            if (node_level > out_level)
                out_level = node_level;

            if (node.isLeaf())
            {
#if DEBUG
                assert(node.itemStart() < this->numItems());
                assert((node.itemStart() + node.numItems()-1) < this->numItems());
#endif

                if (next_to_visit_index == 0)
                    break;
                --next_to_visit_index;
                current_node_index = nodes_to_visit_stack[next_to_visit_index];
            }
            else
            {
                // Put far Bvh node on nodes_to_visit_stack, advance to near node
                if (stx.Rtx.isSlopePositive(node.split_axis))
                {
                    nodes_to_visit_stack[next_to_visit_index++] = node.B_offset;
                    current_node_index = (current_node_index + 1);
                }
                else
                {
                    nodes_to_visit_stack[next_to_visit_index++] = (current_node_index + 1);
                    current_node_index = node.B_offset;
                }
            }

        }
        else
        {
            if (next_to_visit_index == 0)
                break;
            --next_to_visit_index;
            current_node_index = nodes_to_visit_stack[next_to_visit_index];
        }

    }

    //std::cout << "      out_level=" << out_level << std::endl;
    return out_level; // no hits
}


/*virtual*/
template<class T>
inline void
Bvh<T>::getIntersectionsWithUVs(RayShaderContext&          stx,
                                const Fsr::Vec2f&          uv0,
                                const Fsr::Vec2f&          uv1,
                                UVSegmentIntersectionList& I_list)
{
    std::cout << "Bvh<T>::getIntersectionsWithUVs(" << this << ") not implemented!" << std::endl;
}


} // namespace zpr


#endif

// end of zprender/Bvh.h

//
// Copyright 2020 DreamWorks Animation
//
