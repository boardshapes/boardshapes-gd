#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/classes/image.hpp>

namespace godot
{
  class ShapeData : public RefCounted
  {
    GDCLASS(ShapeData, RefCounted)

  private:
    int number;
    Vector2 corner;
    PackedVector2Array path;
    Color color;
    String color_string;
    Ref<Image> image;

  protected:
    static void _bind_methods();

  public:
    ShapeData();
    ~ShapeData();
    int get_number();
    Vector2 get_corner();
    PackedVector2Array get_path();
    Color get_color();
    String get_color_string();
    Ref<Image> get_image();

    friend class BoardshapesData;
  };

  MAKE_TYPED_ARRAY(Ref<ShapeData>, Variant::OBJECT)
  MAKE_TYPED_ARRAY_INFO(Ref<ShapeData>, Variant::OBJECT)

  class BoardshapesData : public RefCounted
  {
    GDCLASS(BoardshapesData, RefCounted)

  private:
    String version;
    TypedArray<Ref<ShapeData>> shapes;

  protected:
    static void _bind_methods();

  public:
    BoardshapesData();
    ~BoardshapesData();
    String get_version();
    TypedArray<Ref<ShapeData>> get_shapes();
    void load_from_binary(PackedByteArray data);
  };
}