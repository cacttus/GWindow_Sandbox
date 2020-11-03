
# Meta-Goop
_Goopy Metaballs_
I got this idea playing Pikmin3. The game has a lot of goopy elements.
Typical metaball formula is take it's cubed radius (scalar field) and iterate over the whole field.
Sum 1/ ( (x-x0)^2 + (y-y0)^2 + (z-z0)^2 )
Each 8 points is 1 voxel.
Create a scalar field on the GPU. We instance invoke a geometry shader on this field.
The field density (voxel size) can be as fine-grained as we want.
We only need 1 of these that we can invoke for every metaball (one 'mesh').
This is perfect for instancing and geometry shaders.
From wiki-GL: The GS can be instanced (this is separate from instanced rendering, as this is localized to the GS). 
              This causes the GS to execute multiple times for the same input primitive. Each invocation of 
              the GS for a particular input primitive will get a different __gl_InvocationID__ value. This is useful
              for layered rendering and outputs to multiple streams
MAX_GEOMETRY_OUTPUT_VERTICES. The minimum value for this limit is 256. * EXCELLENT we will be alright.
MAX_GEOMETRY_SHADER_INVOCATIONS (this will be at least 32). * So at least 32 metaballs at one time. We should use **BATCHED INSTANCING**
  VULKAN: VkPhysicalDeviceLimits.maxGeometryShaderInvocations;
          VkPhysicalDeviceLimits.maxGeometryOutputVertices;

## Simulation
Metaballs are good, but they dont behave like liquid unless you use thousands of metaballs. That would get 
slow and there's a better way. How to simulate liquid without using a ton of metaballs? Use a dynamic parametric equation.
instead of a sphere use a spheroid. The Y axis shrinks based on distnace from the surface of contact.
Collide with the plane underneath the ball. There are 2 radii - collision radius and metaball radius.  
Collision radius is just where the ball sits - the metaball radius is a parametric spheroid.

spheroid - y=1,
  * *
* ^y  * | velocity
* |   * v 
  * *

-------- surface

spheroid y=0.5,
 * * * * *
*    ^ y   *
*    |   ---------surface
 * * * * *  

This can be generalized - instead of Y-axis, use the collision normal and plane distance to parameterize the metaball equation.
How to make the liquid look jiggly?  Turn the collision normal (N) into a spring. Calculate T, B axes, then create the metaball equation.
Use the spring equation (Hooke's law) to calculate the T/B/N and adjust the spring constant to get the desired fluid equillibrium.

## Geometry Shader
```C++
layout (location=0, points) in;  //Make the points floats because we are going to multiply them anyway.
layout (triangle_list, max_vertices = 6*6) out; //VkPhysicalDeviceLimits.maxGeometryOutputVertices;
smooth out vec3 _ptNormal; //Vertex Normal 

struct Metaball{
  vec3 pos; //center of metaball
}
layout (location=0) uniform Metaballs {
  Metaball _mballs[128];
} _mballs;
layout (location=1) uniform MBLookup {
  int voff[256] // index into verts given the pattern
  int tcount[256] // number of triangles in verts[voff[i]]
  ivec3 vind[8*6*256] // index lookup table (into V8) = 8*6*256 indexes. **NOTE: this is the MAXIMUM - we will need much less. This is not uniformly spaced data.
  //vec3 points[8]; // the points where p0 = 0,0,0 and p7 = s,s,s where s is the voxel size
  //NOTE:
  //We should test whether it's faster to have an array of precomputed points adding an offset, OR
  // it's faster to compute the 8 points in the shader and use a table of triangle indexes.
} _lookup; //- block topology lookup-table.
layout(location=2) uniform MBData{
  int _mbIndex; // Index of this metaball in the _mballs aray.
  float _voxelSize; //Size of a voxel
  int _ballCount; // Number of balls in Metaballs array.
  float _threshold;
} _mbData;

//P = m_pts
//      f6*-----*f7 (P7)
//       /     / 
//    f2*---f3*  *f5
//      |  v  | /
//(P0)f0*-----*f1

//Create the base voxel, we could also precompute the vertexes into another table (needs testing)
float vsiz = _mbData._voxelSize;
vec3 m_pos = _mballs._mballs[_mbData._mbIndex].pos;
vec3 m_pt[8]; // the 8 points of the voxel.
m_pt[0] = m_pos + _point * vsiz + vec3(  0,    0,   0);
m_pt[1] = m_pos + _point * vsiz + vec3(vsiz,   0,   0);
m_pt[2] = m_pos + _point * vsiz + vec3(   0,vsiz,   0);
m_pt[3] = m_pos + _point * vsiz + vec3(vsiz,vsiz,   0);
m_pt[4] = m_pos + _point * vsiz + vec3(   0,   0,vsiz);
m_pt[5] = m_pos + _point * vsiz + vec3(vsiz,vsiz,vsiz);
m_pt[6] = m_pos + _point * vsiz + vec3(   0,   0,vsiz);
m_pt[7] = m_pos + _point * vsiz + vec3(vsiz,vsiz,vsiz);

float v[8] = float(0,0,0,0,0,0,0,0);
vec3 m_pt_n[8]; // normals;

//Compute the Scalar Field at (x,y,z)
for(int i=0; i<_mbData._ballCount; i++) {
  vec3 m2_pos = _mballs._mballs[i].pos;
  for(int i=0; i<8; i++){
    // sum(m[i]) = 1/ ( (x-x0)^2 + (y-y0)^2 + (z-z0)^2 ) <= Threshold
    vec3 d = pow(m_pt[i] - m2_pos,2);
    v[i] += 1.0/(d.x + d.y + d.z);

    //normal (pseudocode)
    m_pt_n = v[i] * nv; 

    //Note: This is pseudocode, the BT averages the points over the 
    //cube edges, similar to marching cubes.
    //There are many more indexes as well.
  }
}

//Create an 8-bit index.
float th = _mbData._threshold;
int idx = 
0x01 * int(v[0]<=th) + 
0x02 * int(v[1]<=th) +
0x04 * int(v[2]<=th) +
0x08 * int(v[3]<=th) +
0x10 * int(v[4]<=th) +
0x20 * int(v[5]<=th) +
0x40 * int(v[6]<=th) +
0x80 * int(v[7]<=th) ;

//TODO: Test removing this condition
if(idx == 256 || idx == 0){
  return; // discard
}

//Lookup vertexes & emit
int voff = _lookup.voff[idx] ;
int tcount = _lookup.tcount[idx];
for(int i=0; i<tcount; i+=3) {
  int pt_index = _lookup.vind[voff+i+0];
  gl_Position = m_pt[pt_index];

  _ptNormal = m_pt_n[pt_index];

  EmitVertex();
}
```



