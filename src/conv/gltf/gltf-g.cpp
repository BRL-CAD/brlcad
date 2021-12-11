#include "common.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "tinygltf/tiny_gltf.h"

#include "vmath.h"
//#include "bu/app.h"
//#include "raytrace.h" says no common in bu/avs.h file
#include "wdb.h"
//#include "rt/geom.h" same as others about no common





//using namespace std;


//print geometry (possibly)

//indention
static std::string Indent(const int indent) {
  std::string s;
  for (int i = 0; i < indent; i++) {
    s += "  ";
  }

  return s;
}


static void testingStringIntMap(const std::map<std::string, int> &m, int &pos) {
  std::map<std::string, int>::const_iterator it(m.begin());
  std::map<std::string, int>::const_iterator itEnd(m.end());
  for (; it != itEnd; it++) {
    //std::cout << Indent(indent) << it->first << ": " << it->second << std::endl;
    if(it->first =="POSITION")
    {
    	pos = it->second;
    	std::cout << "BOT position: " << pos << std::endl;
    }
    
  }
}


#ifdef GLTF_DEBUG
static std::string PrintMode(int mode) {
  if (mode == TINYGLTF_MODE_POINTS) {
    return "POINTS";
  } else if (mode == TINYGLTF_MODE_LINE) {
    return "LINE";
  } else if (mode == TINYGLTF_MODE_LINE_LOOP) {
    return "LINE_LOOP";
  } else if (mode == TINYGLTF_MODE_TRIANGLES) {
    return "TRIANGLES";
  } else if (mode == TINYGLTF_MODE_TRIANGLE_FAN) {
    return "TRIANGLE_FAN";
  } else if (mode == TINYGLTF_MODE_TRIANGLE_STRIP) {
    return "TRIANGLE_STRIP";
  }
  return "**UNKNOWN**";
}

static std::string PrintComponentType(int ty) {
  if (ty == TINYGLTF_COMPONENT_TYPE_BYTE) {
    return "BYTE";
  } else if (ty == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
    return "UNSIGNED_BYTE";
  } else if (ty == TINYGLTF_COMPONENT_TYPE_SHORT) {
    return "SHORT";
  } else if (ty == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
    return "UNSIGNED_SHORT";
  } else if (ty == TINYGLTF_COMPONENT_TYPE_INT) {
    return "INT";
  } else if (ty == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
    return "UNSIGNED_INT";
  } else if (ty == TINYGLTF_COMPONENT_TYPE_FLOAT) {
    return "FLOAT";
  } else if (ty == TINYGLTF_COMPONENT_TYPE_DOUBLE) {
    return "DOUBLE";
  }

  return "**UNKNOWN**";
}

static std::string PrintType(int ty) {
  if (ty == TINYGLTF_TYPE_SCALAR) {
    return "SCALAR";
  } else if (ty == TINYGLTF_TYPE_VECTOR) {
    return "VECTOR";
  } else if (ty == TINYGLTF_TYPE_VEC2) {
    return "VEC2";
  } else if (ty == TINYGLTF_TYPE_VEC3) {
    return "VEC3";
  } else if (ty == TINYGLTF_TYPE_VEC4) {
    return "VEC4";
  } else if (ty == TINYGLTF_TYPE_MATRIX) {
    return "MATRIX";
  } else if (ty == TINYGLTF_TYPE_MAT2) {
    return "MAT2";
  } else if (ty == TINYGLTF_TYPE_MAT3) {
    return "MAT3";
  } else if (ty == TINYGLTF_TYPE_MAT4) {
    return "MAT4";
  }
  return "**UNKNOWN**";
}




static std::string PrintIntArray(const std::vector<int> &arr) {
  if (arr.size() == 0) {
    return "";
  }

  std::stringstream ss;
  ss << "[ ";
  for (size_t i = 0; i < arr.size(); i++) {
    ss << arr[i];
    if (i != arr.size() - 1) {
      ss << ", ";
    }
  }
  ss << " ]";

  return ss.str();
}


static std::string PrintFloatArray(const std::vector<double> &arr) {
  if (arr.size() == 0) {
    return "";
  }

  std::stringstream ss;
  ss << "[ ";
  for (size_t i = 0; i < arr.size(); i++) {
    ss << arr[i];
    if (i != arr.size() - 1) {
      ss << ", ";
    }
  }
  ss << " ]";

  return ss.str();
}

static std::string PrintParameterValue(const tinygltf::Parameter &param) {
  if (!param.number_array.empty()) {
    return PrintFloatArray(param.number_array);
  } else {
    return param.string_value;
  }
}

//prints a value
static std::string PrintValue(const std::string &name,
                              const tinygltf::Value &value, const int indent,
                              const bool tag = true) {
  std::stringstream ss;

  if (value.IsObject()) {
    const tinygltf::Value::Object &o = value.Get<tinygltf::Value::Object>();
    tinygltf::Value::Object::const_iterator it(o.begin());
    tinygltf::Value::Object::const_iterator itEnd(o.end());
    for (; it != itEnd; it++) {
      ss << PrintValue(it->first, it->second, indent + 1) << std::endl;
    }
  } else if (value.IsString()) {
    if (tag) {
      ss << Indent(indent) << name << " : " << value.Get<std::string>();
    } else {
      ss << Indent(indent) << value.Get<std::string>() << " ";
    }
  } else if (value.IsBool()) {
    if (tag) {
      ss << Indent(indent) << name << " : " << value.Get<bool>();
    } else {
      ss << Indent(indent) << value.Get<bool>() << " ";
    }
  } else if (value.IsNumber()) {
    if (tag) {
      ss << Indent(indent) << name << " : " << value.Get<double>();
    } else {
      ss << Indent(indent) << value.Get<double>() << " ";
    }
  } else if (value.IsInt()) {
    if (tag) {
      ss << Indent(indent) << name << " : " << value.Get<int>();
    } else {
      ss << Indent(indent) << value.Get<int>() << " ";
    }
  } else if (value.IsArray()) {
    // TODO(syoyo): Better pretty printing of array item
    ss << Indent(indent) << name << " [ \n";
    for (size_t i = 0; i < value.Size(); i++) {
      ss << PrintValue("", value.Get(int(i)), indent + 1, /* tag */ false);
      if (i != (value.ArrayLen() - 1)) {
        ss << ", \n";
      }
    }
    ss << "\n" << Indent(indent) << "] ";
  }

  // @todo { binary }

  return ss.str();
}
#endif

#ifdef GLTF_DEBUG

static void testingStringIntMap(const std::map<std::string, int> &m, int &pos) {
  std::map<std::string, int>::const_iterator it(m.begin());
  std::map<std::string, int>::const_iterator itEnd(m.end());
  for (; it != itEnd; it++) {
    //std::cout << Indent(indent) << it->first << ": " << it->second << std::endl;
    if(it->first =="POSITION")
    {
    	pos = it->second;
    	std::cout << "BOT position: " << pos << std::endl;
    }
    
  }
}

static void testingExtensions(const tinygltf::ExtensionMap &extension,
                           const int indent) {
  // TODO(syoyo): pritty print Value
  for (auto &e : extension) {
    std::cout << Indent(indent) << e.first << std::endl;
    std::cout << PrintValue("extensions", e.second, indent + 1) << std::endl;
  }
}


static void testingNode(const tinygltf::Node &node, int indent) {
  std::cout << Indent(indent) << "name        : " << node.name << std::endl;
  std::cout << Indent(indent) << "camera      : " << node.camera << std::endl;
  std::cout << Indent(indent) << "mesh        : " << node.mesh << std::endl;
  if (!node.rotation.empty()) {
    std::cout << Indent(indent)
              << "rotation    : " << PrintFloatArray(node.rotation)
              << std::endl;
  }
  if (!node.scale.empty()) {
    std::cout << Indent(indent)
              << "scale       : " << PrintFloatArray(node.scale) << std::endl;
  }
  if (!node.translation.empty()) {
    std::cout << Indent(indent)
              << "translation : " << PrintFloatArray(node.translation)
              << std::endl;
  }

  if (!node.matrix.empty()) {
    std::cout << Indent(indent)
              << "matrix      : " << PrintFloatArray(node.matrix) << std::endl;
  }

  std::cout << Indent(indent)
            << "children    : " << PrintIntArray(node.children) << std::endl;
}


static void testingPrimitive(const tinygltf::Primitive &primitive, int indent) {
  std::cout << Indent(indent) << "material : " << primitive.material
            << std::endl;
  std::cout << Indent(indent) << "indices : " << primitive.indices << std::endl;
  std::cout << Indent(indent) << "mode     : " << PrintMode(primitive.mode)
            << "(" << primitive.mode << ")" << std::endl;
  std::cout << Indent(indent)
            << "attributes(items=" << primitive.attributes.size() << ")"
            << std::endl;
  testingStringIntMap(primitive.attributes, indent + 1);

  testingExtensions(primitive.extensions, indent);
  std::cout << Indent(indent) << "extras :" << std::endl
            << PrintValue("extras", primitive.extras, indent + 1) << std::endl;

  if (!primitive.extensions_json_string.empty()) {
    std::cout << Indent(indent + 1) << "extensions(JSON string) = "
              << primitive.extensions_json_string << "\n";
  }

  if (!primitive.extras_json_string.empty()) {
    std::cout << Indent(indent + 1)
              << "extras(JSON string) = " << primitive.extras_json_string
              << "\n";
  }
}

#endif


static void getshapename(const tinygltf::Model &model, std::string &name)
{
    
      std::cout << Indent(1) << "name     : " << model.meshes[0].name
                  << std::endl;
      std::string temp = model.meshes[0].name;
      name.append(model.meshes[0].name);


}

static void gathershapeinfo(const tinygltf::Model &model, int &numvert, int &numfaces)
{
  //buffer if needed
  std::cout << "Mesh size: " << model.meshes.size() << std::endl;
{
  
    int indices_pos = model.meshes[0].primitives[0].indices;

  //assume 1 accessor, 1 bufferview
  const tinygltf::Accessor &accessor = model.accessors[indices_pos];
  const tinygltf::BufferView &bufferView = model.bufferViews[indices_pos];
  //assume 1 accessor, 1 bufferview
  const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
  //unsigned char to short int arr
  const unsigned char * dataPtr = buffer.data.data() + bufferView.byteOffset +
                                  accessor.byteOffset;
  const int byte_stride = accessor.ByteStride(bufferView);
  //const size_t count = accessor.count;

  unsigned short* indices = (unsigned short*) dataPtr;
  int *faces = new int[bufferView.byteLength / byte_stride];
  //int faces[bufferView.byteLength / byte_stride] ;
  numfaces = bufferView.byteLength / byte_stride;

  for(long unsigned int i = 0; i < bufferView.byteLength / byte_stride; i++)
  {
      //std::cout << "i : " << i  << " = " << indices[i] << std::endl;
      faces[i] = indices[i];

  }

  std::cout << "Number of Faces: " << numfaces << std::endl;
}

{
  int bot_pos;
  testingStringIntMap(model.meshes[0].primitives[0].attributes, bot_pos);

  //assume 1 accessor, 1 bufferview
  const tinygltf::Accessor &accessor = model.accessors[bot_pos];
  const tinygltf::BufferView &bufferView = model.bufferViews[bot_pos];
  const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
  //unsigned char to short int arr
  const unsigned char * dataPtr = buffer.data.data() + bufferView.byteOffset +
                                  accessor.byteOffset;
  const int byte_stride = accessor.ByteStride(bufferView);
  //const size_t count = accessor.count;

  float* positions = (float*) dataPtr;
  //change double to fast_f for mk_bot
  double *vertices = new double[(bufferView.byteLength / byte_stride) * 3]; 
  //double vertices[(bufferView.byteLength / byte_stride) * 3] ;
  numvert = (bufferView.byteLength / byte_stride) * 3;
  
  for(long unsigned int i = 0; i < bufferView.byteLength / byte_stride; i++)
  {
      std::cout << "i : " << i  << " = " << positions[i*3] << " , "
      << positions[(i*3) + 1] << " , " << positions[(i*3) + 2] << std::endl;

      vertices[i*3] = positions[i*3];
      vertices[(i*3)+1] = positions[(i*3)+1];
      vertices[(i*3)+2] = positions[(i*3)+2];


  }
  std::cout << "Number of vertices: " << numvert << std::endl;
  std:: cout << "Stride Count : " << byte_stride << std::endl;

}
return ;

}

static void insertvectorfaces(const tinygltf::Model &model, double vertices[], int faces[])
{
  
{

  int indices_pos = model.meshes[0].primitives[0].indices;

  //assume 1 accessor, 1 bufferview
  const tinygltf::Accessor &accessor = model.accessors[indices_pos];
  const tinygltf::BufferView &bufferView = model.bufferViews[indices_pos];
  const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
  //unsigned char to short int arr
  const unsigned char * dataPtr = buffer.data.data() + bufferView.byteOffset +
                                  accessor.byteOffset;
  const int byte_stride = accessor.ByteStride(bufferView);
  //const size_t count = accessor.count;

  unsigned short* indices = (unsigned short*) dataPtr;
  //int faces[bufferView.byteLength / byte_stride] ;
  int numfaces = bufferView.byteLength / byte_stride;

  for(long unsigned int i = 0; i < bufferView.byteLength / byte_stride; i++)
  {
      //std::cout << "i : " << i  << " = " << indices[i] << std::endl;
      faces[i] = indices[i];

  }

  std::cout << "Number of Faces: " << numfaces << std::endl;
}

{
  int bot_pos;
  testingStringIntMap(model.meshes[0].primitives[0].attributes, bot_pos);

  //assume 1 accessor, 1 bufferview
  const tinygltf::Accessor &accessor = model.accessors[bot_pos];
  const tinygltf::BufferView &bufferView = model.bufferViews[bot_pos];
  const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
  //unsigned char to short int arr
  const unsigned char * dataPtr = buffer.data.data() + bufferView.byteOffset +
                                  accessor.byteOffset;
  const int byte_stride = accessor.ByteStride(bufferView);
  //const size_t count = accessor.count;

  float* positions = (float*) dataPtr;
  //change double to fast_f for mk_bot
  //double vertices[(bufferView.byteLength / byte_stride) * 3] ;
  int numvert = (bufferView.byteLength / byte_stride) * 3;
  
  for(long unsigned int i = 0; i < bufferView.byteLength / byte_stride; i++)
  {
      std::cout << "i : " << i  << " = " << positions[i*3] << " , "
      << positions[(i*3) + 1] << " , " << positions[(i*3) + 2] << std::endl;

      vertices[i*3] = positions[i*3];
      vertices[(i*3)+1] = positions[(i*3)+1];
      vertices[(i*3)+2] = positions[(i*3)+2];


  }
  std::cout << "Number of vertices: " << numvert << std::endl;
  std:: cout << "Stride Count : " << byte_stride << std::endl;

}
return ;

}

static std::string GetFilePathExtension(const std::string &FileName) 
{
  if (FileName.find_last_of(".") != std::string::npos)
  {
    return FileName.substr(FileName.find_last_of(".") + 1);
  }
  return "";
}


int main(int argc, char **argv)
{
    bool store_original_json_for_extras_and_extensions = false;
    
    if (argc < 2) 
    {
        printf("Needs input.gltf\n");
        exit(1);
    }
    
    if (argc > 2) {
        store_original_json_for_extras_and_extensions = true;
    }
    

    tinygltf::Model model;
    std::string err;
    std::string warn;
    std::string input_filename(argv[1]);
    std::string ext = GetFilePathExtension(input_filename);


    bool ret = false;

    // getting undefined reference here for the class
    tinygltf::TinyGLTF gltf_ctx;

    gltf_ctx.SetStoreOriginalJSONForExtrasAndExtensions(
        store_original_json_for_extras_and_extensions);


    if (ext.compare("glb") == 0) {
    std::cout << "File type: binary glTF" << std::endl;
    // assume binary glTF.
    ret = gltf_ctx.LoadBinaryFromFile(&model, &err, &warn,
                                      input_filename);
  } else {
    std::cout << "File type: ASCII glTF" << std::endl;
    // assume ascii glTF.
    ret =
        gltf_ctx.LoadASCIIFromFile(&model, &err, &warn, input_filename.c_str());
  }

    if (!warn.empty()) {
        printf("Warn: %s\n", warn.c_str());
    }

    if (!err.empty()) {
        printf("Err: %s\n", err.c_str());
    }

    if (!ret) {
        printf("Failed to parse glTF\n");
        return -1;
    }

  
 std::string shape_name;

  getshapename(model, shape_name);

  std::cout << "Shape name: " << shape_name << std::endl;
 
//array of vectors, edges, faces

  int num_vert_values;
  int num_face_values;
  
  gathershapeinfo(model, num_vert_values, num_face_values);
  
  std::cout << "Num vert : " << num_vert_values / 3 << std::endl;
  std::cout << "Num faces: " << num_face_values / 3 << std::endl;

  int *faces = new int[num_face_values*3];
  double *vertices = new double[num_vert_values*3];
  //int faces[num_faces];
  //double vertices[num_vert_values];
  
  insertvectorfaces(model, vertices, faces);

   for(int i = 0; i < num_vert_values; i = i + 3)
  {
  	//std::cout << vertices[i] << "," << vertices[i+1] << "," << vertices[i+2] << std::endl;
  }

  for(int i = 0; i < num_face_values; i = i + 3)
  {
  	//std::cout << faces[i] << "," << faces[i+1] << "," << faces[i+2] << std::endl;
  }
  
// mk_Bot needs file, name, num vert, num face, verts, faces
struct rt_wdb *outfp = NULL;

//need to fix this. replace test.g with new
std::string gfile;
gfile = shape_name + ".g";

outfp = wdb_fopen(gfile.c_str());

std::string title = "gltf conversion from" + input_filename;

mk_id(outfp, title.c_str());

mk_bot(outfp, shape_name.c_str(), RT_BOT_SURFACE, RT_BOT_UNORIENTED,0, num_vert_values / 3, num_face_values / 3, vertices, faces, (fastf_t *)NULL,(struct bu_bitv *)NULL);

 
  wdb_close(outfp);
  

    return 0;


}

