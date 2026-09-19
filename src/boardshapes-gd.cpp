#include "boardshapes-gd.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/classes/bit_map.hpp>

using namespace godot;

#define GET_BIG_ENDIAN_UINT32(arr, i) (arr[i] << 24) | (arr[i + 1] << 16) | (arr[i + 2] << 8) | arr[i + 3]
#define GET_BIG_ENDIAN_UINT16(arr, i) (arr[i] << 8) | arr[i + 1]

const int CHUNK_VERSION = 0;
const int CHUNK_COLOR_TABLE = 2;
const int CHUNK_SHAPE_GEOMETRY = 8;
const int CHUNK_SHAPE_COLOR = 9;
const int CHUNK_SHAPE_IMAGE = 10;
const int CHUNK_SHAPE_MASK = 11;

void BoardshapesData::_bind_methods()
{
  ClassDB::bind_method(D_METHOD("get_version"), &BoardshapesData::get_version);
  ClassDB::bind_method(D_METHOD("get_shapes"), &BoardshapesData::get_shapes);
  ClassDB::bind_method(D_METHOD("load_from_binary", "data"), &BoardshapesData::load_from_binary);

  ADD_PROPERTY(PropertyInfo(Variant::STRING, "version"), "", "get_version");
  ADD_PROPERTY(
      PropertyInfo(
          Variant::ARRAY,
          "shapes"),
      "",
      "get_shapes");
}

String BoardshapesData::get_version()
{
  return version;
}

TypedArray<Ref<ShapeData>> BoardshapesData::get_shapes()
{
  return shapes;
}

void BoardshapesData::load_from_binary(PackedByteArray data)
{
  ERR_FAIL_COND_MSG(data[0] != CHUNK_VERSION, "Boardshapes data without an initial VERSION chunk is unsupported.");

  int64_t pos = 0;
  int64_t size = data.size();
  HashMap<Color, String> colorTable{};
  HashMap<uint32_t, Ref<ShapeData>> shapesMap{};
  HashMap<uint32_t, Ref<BitMap>> shapeMasks{};

  while (pos < size)
  {
    uint8_t chunkNumber = data[pos++];

    switch (chunkNumber)
    {
    case CHUNK_VERSION:
    {
      int64_t stringEnd = data.find(0, pos);

      ERR_FAIL_COND_MSG(stringEnd == -1, "Version string is not null-terminated.");
      ERR_FAIL_COND(stringEnd < pos || stringEnd >= size);

      version = data.slice(pos, stringEnd).get_string_from_utf8();

      pos = stringEnd + 1;
    }
    break;
    case CHUNK_COLOR_TABLE:
    {
      ERR_FAIL_INDEX_MSG(pos + 3, size, "Color table chunk not large enough.");
      uint32_t nColors = GET_BIG_ENDIAN_UINT32(data, pos);

      pos += 4;
      for (size_t i = 0; i < nColors; i++)
      {
        ERR_FAIL_INDEX_MSG(pos + 4, size, "Color table chunk not large enough.");
        Color color{data[pos] / 255.0f, data[pos + 1] / 255.0f, data[pos + 2] / 255.0f, data[pos + 3] / 255.0f};

        pos += 4;
        int64_t stringEnd = data.find(0, pos);
        ERR_FAIL_COND_MSG(stringEnd == -1, "Color name string is not null-terminated.");
        ERR_FAIL_COND(stringEnd < pos || stringEnd >= size);

        String colorName = data.slice(pos, stringEnd).get_string_from_utf8();
        colorTable.insert(color, colorName);
        pos = stringEnd + 1;
      }
    }
    break;
    case CHUNK_SHAPE_GEOMETRY:
    case CHUNK_SHAPE_COLOR:
    case CHUNK_SHAPE_IMAGE:
    case CHUNK_SHAPE_MASK:
    {
      ERR_FAIL_INDEX_MSG(pos + 4, size, "Shape chunk not large enough.");

      uint32_t shapeNumber = GET_BIG_ENDIAN_UINT32(data, pos);

      Ref<ShapeData> shape;
      if (shapesMap.has(shapeNumber))
      {
        shape = shapesMap[shapeNumber];
      }
      else
      {
        shape = Ref<ShapeData>(memnew(ShapeData));
        shape->number = shapeNumber;
        shapesMap.insert(shapeNumber, shape);
      }

      pos += 4;

      switch (chunkNumber)
      {
      case CHUNK_SHAPE_GEOMETRY:
      {
        ERR_FAIL_INDEX_MSG(pos + 7, size, "Shape geometry chunk not large enough.");

        uint16_t cornerX = GET_BIG_ENDIAN_UINT16(data, pos);
        uint16_t cornerY = GET_BIG_ENDIAN_UINT16(data, pos + 2);
        uint32_t nVertices = GET_BIG_ENDIAN_UINT32(data, pos + 4);

        shape->corner = Vector2(cornerX, cornerY);

        pos += 8;
        ERR_FAIL_INDEX_MSG((pos - 1) + 4 * nVertices, size, "Shape geometry chunk not large enough for all shapes.");

        PackedVector2Array path{};
        path.resize(nVertices);
        for (size_t i = 0; i < nVertices; i++)
        {
          path[i].x = GET_BIG_ENDIAN_UINT16(data, pos);
          path[i].y = GET_BIG_ENDIAN_UINT16(data, pos + 2);
          pos += 4;
        }
        shape->path = path;
      }
      break;
      case CHUNK_SHAPE_COLOR:
      {
        ERR_FAIL_INDEX_MSG(pos + 3, size, "Shape color chunk not large enough.");
        Color color{data[pos] / 255.0f, data[pos + 1] / 255.0f, data[pos + 2] / 255.0f, data[pos + 3] / 255.0f};
        shape->color = color;
        pos += 4;
      }
      break;
      case CHUNK_SHAPE_IMAGE:
      {
        ERR_FAIL_INDEX_MSG(pos + 3, size, "Shape image chunk not large enough.");

        uint32_t nBytes = GET_BIG_ENDIAN_UINT32(data, pos);
        pos += 4;

        if (nBytes > 0)
        {
          ERR_FAIL_INDEX_MSG((pos - 1) + nBytes, size, "Shape image chunk not large enough.");

          Ref<Image> image = memnew(Image);

          image->load_png_from_buffer(data.slice(pos, pos + nBytes));

          shape->image = image;

          pos += nBytes;
        }
      }
      break;
      case CHUNK_SHAPE_MASK:
      {
        ERR_FAIL_INDEX_MSG(pos + 2, size, "Shape mask chunk not large enough.");
        uint16_t maskWidth = GET_BIG_ENDIAN_UINT16(data, pos);
        ERR_FAIL_COND_MSG(maskWidth == 0, "Shape mask width is 0 which is not valid.");

        bool filled = data[pos + 2] != 0;
        pos += 3;

        int64_t chunkEnd = data.find(0, pos);
        ERR_FAIL_COND_MSG(chunkEnd == -1, "Shape mask chunk is not null-terminated.");
        ERR_FAIL_COND(chunkEnd < pos || chunkEnd >= size);

        if (chunkEnd > pos)
        {
          LocalVector<size_t> runLengths;
          runLengths.reserve(chunkEnd - pos);

          for (; pos < chunkEnd; pos++)
          {
            uint8_t b1 = data[pos];

            uint8_t a = b1 & 0b10000000;
            if ((b1 & 0b10000000) == 0)
            {
              runLengths.push_back(b1 & 0b01111111);
              continue;
            }

            pos++;
            ERR_FAIL_INDEX_MSG(pos, chunkEnd, "Shape mask chunk does not contain valid var-ints.");

            uint8_t b2 = data[pos];
            if ((b2 & 0b10000000) == 0)
            {
              runLengths.push_back(((b2 & 0b01111111) << 7) | b1 & 0b01111111);
              continue;
            }

            pos++;
            ERR_FAIL_INDEX_MSG(pos, chunkEnd, "Shape mask chunk does not contain valid var-ints.");

            uint8_t b3 = data[pos];
            if ((b3 & 0b10000000) == 0)
            {
              runLengths.push_back(((b3 & 0b01111111) << 14) | ((b2 & 0b01111111) << 7) | b1 & 0b01111111);
              continue;
            }

            pos++;
            ERR_FAIL_INDEX_MSG(pos, chunkEnd, "Shape mask chunk does not contain valid var-ints.");

            uint8_t b4 = data[pos];
            runLengths.push_back(((b4 & 0b01111111) << 21) | ((b3 & 0b01111111) << 14) | ((b2 & 0b01111111) << 7) | b1 & 0b01111111);
            ERR_FAIL_COND_MSG((b4 & 0b10000000) != 0, "Shape mask chunk contains a var-int that is too large.");
          }
          pos++;

          size_t sum{0};
          for (auto &&runLength : runLengths)
          {
            sum += runLength;
          }

          ERR_FAIL_COND_MSG(sum % maskWidth != 0, "Shape mask width does not divide evenly into total number of pixels in mask.");
          size_t maskHeight = sum / maskWidth;

          size_t x{0}, y{0};
          Ref<BitMap> maskBmp = memnew(BitMap);
          maskBmp->create(Vector2i{static_cast<int32_t>(maskWidth), static_cast<int32_t>(maskHeight)});
          for (auto &&runLength : runLengths)
          {
            size_t nx = x + runLength;
            if (nx >= maskWidth)
            {
              // fill to end of row
              if (filled)
              {
                maskBmp->set_bit_rect(Rect2i{static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(maskWidth - x), 1}, true);
              }

              y++;
              nx -= maskWidth;
              size_t h = nx / maskWidth;
              // fill fully filled rows
              if (filled && h > 0)
              {
                maskBmp->set_bit_rect(Rect2i{0, static_cast<int32_t>(y), static_cast<int32_t>(maskWidth), static_cast<int32_t>(h)}, true);
              }

              y += h;
              x = nx % maskWidth;

              // fill part of last row if necessary
              if (filled && x > 0)
              {
                maskBmp->set_bit_rect(Rect2i{0, static_cast<int32_t>(y), static_cast<int32_t>(x), 1}, true);
              }
            }
            else
            {
              if (filled)
              {
                maskBmp->set_bit_rect(Rect2i{static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(nx - x), 1}, true);
              }

              x = nx;
            }

            filled = !filled;
          }

          shapeMasks[shapeNumber] = maskBmp;
        }
      }
      break;
      default:
        ERR_FAIL_MSG(vformat("Unknown chunk type %d", chunkNumber));
        break;
      }
    }
    break;
    default:
      ERR_FAIL_MSG(vformat("Unknown chunk type %d", chunkNumber));
      break;
    }
  }

  // create images from masks
  for (auto &&kv : shapeMasks)
  {
    auto mask = kv.value;
    auto shape = shapesMap[kv.key];
    Color color = shape->color;

    Vector2i size = mask->get_size();
    Ref<Image> image = Image::create(size.x, size.y, false, Image::FORMAT_RGBA8);

    for (size_t i = 0; i < size.x; i++)
    {
      for (size_t j = 0; j < size.y; j++)
      {
        image->set_pixel(i, j, mask->get_bit(i, j) ? color : Color(0.0f, 0.0f, 0.0f, 0.0f));
      }
    }

    shape->image = image;
  }

  // add color name strings
  for (auto &&kv : shapesMap)
  {
    auto shape = kv.value;
    if (colorTable.has(shape->color))
    {
      shape->color_string = colorTable[shape->color];
    }
  }

  shapes = TypedArray<Ref<ShapeData>>{};
  shapes.resize(shapesMap.size());

  auto iMap = shapesMap.begin();
  size_t mapSize = shapesMap.size();
  for (size_t i = 0; i < mapSize; i++, ++iMap)
  {
    shapes[i] = iMap->value;
  }
}

BoardshapesData::BoardshapesData()
{
}

BoardshapesData::~BoardshapesData()
{
}

void godot::ShapeData::_bind_methods()
{
  ClassDB::bind_method(D_METHOD("get_number"), &ShapeData::get_number);
  ClassDB::bind_method(D_METHOD("get_corner"), &ShapeData::get_corner);
  ClassDB::bind_method(D_METHOD("get_path"), &ShapeData::get_path);
  ClassDB::bind_method(D_METHOD("get_color"), &ShapeData::get_color);
  ClassDB::bind_method(D_METHOD("get_color_string"), &ShapeData::get_color_string);
  ClassDB::bind_method(D_METHOD("get_image"), &ShapeData::get_image);

  ADD_PROPERTY(PropertyInfo(Variant::INT, "number"), "", "get_number");
  ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "corner"), "", "get_corner");
  ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "path"), "", "get_path");
  ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "", "get_color");
  ADD_PROPERTY(PropertyInfo(Variant::STRING, "color_string"), "", "get_color_string");
  ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "image", PROPERTY_HINT_OBJECT_ID), "", "get_image");
}

ShapeData::ShapeData()
{
}

ShapeData::~ShapeData()
{
}

int ShapeData::get_number()
{
  return number;
}

Vector2 ShapeData::get_corner()
{
  return corner;
}

PackedVector2Array ShapeData::get_path()
{
  return path;
}

Color ShapeData::get_color()
{
  return color;
}

String ShapeData::get_color_string()
{
  return color_string;
}

Ref<Image> ShapeData::get_image()
{
  return image;
}

#undef GET_BIG_ENDIAN_UINT32
#undef GET_BIG_ENDIAN_UINT16