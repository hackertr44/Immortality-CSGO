﻿#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include <tuple>

#define PBFIELD(number, type, name) \
  pbfield<number, type> name() const { \
    return std::move(pbfield<number, type>( fv_ )); \
  }

#if defined(__GNUC__)
  #include <cstring>
  #include <memory>


  #define __forceinline __attribute__((always_inline))

  #define PBMSG_CTOR(class_name) \
      class_name() : pbmsg() {} \
      class_name(void* buf, size_t bytes) : pbmsg(buf, bytes) {} \
      class_name(const std::string& buf) : pbmsg(buf) {}

#elif defined(_MSC_VER)
#define PBMSG_CTOR using pbmsg::pbmsg
#endif

namespace pbwrap
{
  static constexpr int32_t k_invalid_id = -1;

  enum e_field_type
  {
    zTYPE_DOUBLE = 1,
    zTYPE_FLOAT = 2,
    zTYPE_INT64 = 3,
    zTYPE_UINT64 = 4,
    zTYPE_INT32 = 5,
    zTYPE_FIXED64 = 6,
    zTYPE_FIXED32 = 7,
    zTYPE_BOOL = 8,
    zTYPE_STRING = 9,
    zTYPE_GROUP = 10,
    zTYPE_MESSAGE = 11,
    zTYPE_BYTES = 12,
    zTYPE_UINT32 = 13,
    zTYPE_ENUM = 14,
    zTYPE_SFIXED32 = 15,
    zTYPE_SFIXED64 = 16,
    zTYPE_SINT32 = 17,
    zTYPE_SINT64 = 18,
    zMAX_FIELD_TYPE = 18,
  };

  enum e_wire_type
  {
    zWIRETYPE_VARINT = 0,
    zWIRETYPE_FIXED64 = 1,
    zWIRETYPE_LENGTH_DELIMITED = 2,
    zWIRETYPE_START_GROUP = 3,
    zWIRETYPE_END_GROUP = 4,
    zWIRETYPE_FIXED32 = 5,
  };

  static constexpr e_wire_type k_wire_type_for_field_type[zMAX_FIELD_TYPE + 1] = {
    static_cast<e_wire_type>(k_invalid_id), // invalid
    zWIRETYPE_FIXED64, // TYPE_DOUBLE
    zWIRETYPE_FIXED32, // TYPE_FLOAT
    zWIRETYPE_VARINT, // TYPE_INT64
    zWIRETYPE_VARINT, // TYPE_UINT64
    zWIRETYPE_VARINT, // TYPE_INT32
    zWIRETYPE_FIXED64, // TYPE_FIXED64
    zWIRETYPE_FIXED32, // TYPE_FIXED32
    zWIRETYPE_VARINT, // TYPE_BOOL
    zWIRETYPE_LENGTH_DELIMITED, // TYPE_STRING
    zWIRETYPE_START_GROUP, // TYPE_GROUP
    zWIRETYPE_LENGTH_DELIMITED, // TYPE_MESSAGE
    zWIRETYPE_LENGTH_DELIMITED, // TYPE_BYTES
    zWIRETYPE_VARINT, // TYPE_UINT32
    zWIRETYPE_VARINT, // TYPE_ENUM
    zWIRETYPE_FIXED32, // TYPE_SFIXED32
    zWIRETYPE_FIXED64, // TYPE_SFIXED64
    zWIRETYPE_VARINT, // TYPE_SINT32
    zWIRETYPE_VARINT, // TYPE_SINT64
  };

  namespace utils
  {
    static constexpr int k_tag_type_bits = 3;
    static constexpr uint32_t k_tag_type_mask = (1 << k_tag_type_bits) - 1;
    static constexpr int k_max_varint_bytes = 10;
    static constexpr int k_max_varint32_bytes = 5;

    __forceinline auto make_tag(uint32_t field_number, uint32_t wire_type) -> uint32_t
    {
      return static_cast<uint32_t>((field_number << k_tag_type_bits) | wire_type);
    }

    static auto get_bytes_varint32(uint32_t value) -> std::string
    {
      uint8_t bytes[k_max_varint32_bytes];
      int size = 0;
      while (value > 0x7F)
      {
        bytes[size++] = (static_cast<uint8_t>(value) & 0x7F) | 0x80;
        value >>= 7;
      }
      bytes[size++] = static_cast<uint8_t>(value) & 0x7F;
      return std::string{reinterpret_cast<const char*>(&bytes[0]), (size_t)size};
    }

    static auto get_bytes_varint64(uint64_t value) -> std::string
    {
      uint8_t bytes[k_max_varint_bytes];
      int size = 0;
      while (value > 0x7F)
      {
        bytes[size++] = (static_cast<uint8_t>(value) & 0x7F) | 0x80;
        value >>= 7;
      }
      bytes[size++] = static_cast<uint8_t>(value) & 0x7F;
      return std::string{reinterpret_cast<const char*>(&bytes[0]), (size_t)size};
    }

    static auto read_varuint32(const void* data, size_t& bytes_read) -> uint32_t
    {
      auto ptr = reinterpret_cast<const uint8_t*>(data);
      auto value = 0u;
      auto bytes = 0u;

      do
      {
        value |= static_cast<uint32_t>(*ptr & 0x7f) << (7 * bytes);
        bytes++;
      }
      while (*(ptr++) & 0x80 && bytes <= 5);

      bytes_read = bytes;
      return value;
    }

    static auto read_varuint64(const void* data, size_t& bytes_read) -> uint64_t
    {
      auto ptr = reinterpret_cast<const uint8_t*>(data);
      auto value = 0ull;
      auto bytes = 0u;

      do
      {
        value |= static_cast<uint64_t>(*ptr & 0x7f) << (7 * bytes);
        bytes++;
      }
      while (*(ptr++) & 0x80 && bytes <= 10);

      bytes_read = bytes;

      return value;
    }

    // parse field info from data stream
    // --return type => std::tie(field, wire_type, field_value_bytes, field_full_bytes)
    //
    // --field - field id
    // --wire_type - field size type
    // --field_value_bytes - include ONLY value bytes
    // --field_full_bytes - include tag and value bytes
    static auto read_field(const void* data, size_t& bytes_read)
      -> std::tuple<uint32_t, uint32_t, std::string, std::string>
    {
      uint32_t field = *reinterpret_cast<const uint16_t*>(data);
      uint32_t wire_type = field & k_tag_type_mask;

      if (field == 0xffff)
      {
        bytes_read = 0;
        return std::make_tuple(k_invalid_id, k_invalid_id, "", "");
      }

      if (field & 0x80)
      {
        field = ((field & 0x7f) | ((field & 0xff00) >> 1)) >> k_tag_type_bits;
        bytes_read = 2;
      }
      else
      {
        field = (field & 0xff) >> k_tag_type_bits;
        bytes_read = 1;
      }

      std::string value_bytes, full_bytes;
      size_t length, size_delimited;

      switch (wire_type)
      {
      case zWIRETYPE_VARINT:
        read_varuint64((void*)((ptrdiff_t)data + bytes_read), length);
        value_bytes = std::string{reinterpret_cast<const char*>((void*)((ptrdiff_t)data + bytes_read)), length};
        full_bytes = std::string{reinterpret_cast<const char*>(data), bytes_read + length};
        bytes_read += length;
        break;
      case zWIRETYPE_FIXED64:
        value_bytes = std::string{reinterpret_cast<const char*>((void*)((ptrdiff_t)data + bytes_read)), 8};
        full_bytes = std::string{reinterpret_cast<const char*>(data), bytes_read + 8};
        bytes_read += 8;
        break;
      case zWIRETYPE_LENGTH_DELIMITED:
        size_delimited = read_varuint32((void*)((ptrdiff_t)data + bytes_read), length);
        value_bytes = std::string{
          reinterpret_cast<const char*>((void*)((ptrdiff_t)data + bytes_read)), length + size_delimited
        };
        full_bytes = std::string{reinterpret_cast<const char*>(data), bytes_read + length + size_delimited};
        bytes_read += length + size_delimited;
        break;
      case zWIRETYPE_START_GROUP:
        throw std::runtime_error("WIRETYPE_START_GROUP not implemented");
        break;
      case zWIRETYPE_END_GROUP:
        throw std::runtime_error("WIRETYPE_END_GROUP not implemented");
        break;
      case zWIRETYPE_FIXED32:
        value_bytes = std::string{reinterpret_cast<const char*>((void*)((ptrdiff_t)data + bytes_read)), 4};
        full_bytes = std::string{reinterpret_cast<const char*>(data), bytes_read + 4};
        bytes_read += 4;
        break;
      default:
        throw std::runtime_error("Unknown wire type");
        break;
      }

      return std::make_tuple(field, wire_type, std::move(value_bytes), std::move(full_bytes));
    }

    template <typename T>
    auto get_field_bytes(uint32_t field, uint32_t wire_type, T value)
      -> std::pair<std::string, std::string>
    {
      const auto tag = get_bytes_varint32(make_tag(field, wire_type));

      std::string bytes{};
      switch (wire_type)
      {
      case zWIRETYPE_VARINT:
        bytes += get_bytes_varint64(static_cast<uint64_t>(value));
        break;
      case zWIRETYPE_FIXED32:
        bytes += std::string{reinterpret_cast<const char*>(&value), 4};
        break;
      case zWIRETYPE_FIXED64:
        bytes += std::string{reinterpret_cast<const char*>(&value), 8};
        break;
      }

      return {bytes, tag + bytes};
    }

    template <>
    inline auto get_field_bytes<std::string>(uint32_t field, uint32_t wire_type, std::string value)
      -> std::pair<std::string, std::string>
    {
      const auto tag = get_bytes_varint32(make_tag(field, wire_type));

      std::string bytes{};
      bytes += get_bytes_varint32(value.size());
      bytes += value;

      return {bytes, tag + bytes};
    }

    template <>
    inline auto get_field_bytes<const char*>(uint32_t field, uint32_t wire_type, const char* value)
      -> std::pair<std::string, std::string>
    {
      const auto tag = get_bytes_varint32(make_tag(field, wire_type));

      std::string bytes{};
      bytes += get_bytes_varint32(strlen(value));
      bytes += value;

      return {bytes, tag + bytes};
    }
  } // utils

  namespace types
  {
    struct Int32
    {
      static constexpr e_field_type field_type = zTYPE_INT32;
      using type = int32_t;

      static __forceinline auto get(const std::string& value) -> int32_t
      {
        size_t bytes_read;
        return utils::read_varuint32(value.data(), bytes_read);
      }
    };

    struct Uint32
    {
      static constexpr e_field_type field_type = zTYPE_UINT32;
      using type = uint32_t;

      static __forceinline auto get(const std::string& value) -> uint32_t
      {
        size_t bytes_read;
        return utils::read_varuint32(value.data(), bytes_read);
      }
    };

    struct Int64
    {
      static constexpr e_field_type field_type = zTYPE_INT64;
      using type = int64_t;

      static __forceinline auto get(const std::string& value) -> int64_t
      {
        size_t bytes_read;
        return utils::read_varuint64(value.data(), bytes_read);
      }
    };

    struct Uint64
    {
      static constexpr e_field_type field_type = zTYPE_UINT64;
      using type = uint64_t;

      static __forceinline auto get(const std::string& value) -> uint64_t
      {
        size_t bytes_read;
        return utils::read_varuint64(value.data(), bytes_read);
      }
    };

    struct Float
    {
      static constexpr e_field_type field_type = zTYPE_FLOAT;
      using type = float;

      static __forceinline auto get(const std::string& value) -> float
      {
        return *reinterpret_cast<const float*>(value.data());
      }
    };

    struct Double
    {
      static constexpr e_field_type field_type = zTYPE_DOUBLE;
      using type = double;

      static __forceinline auto get(const std::string& value) -> double
      {
        return *reinterpret_cast<const double*>(value.data());
      }
    };

    struct Fixed32
    {
      static constexpr e_field_type field_type = zTYPE_FIXED32;
      using type = int32_t;

      static __forceinline auto get(const std::string& value) -> int32_t
      {
        return *reinterpret_cast<const int32_t*>(value.data());
      }
    };

    struct Fixed64
    {
      static constexpr e_field_type field_type = zTYPE_FIXED64;
      using type = int64_t;

      static __forceinline auto get(const std::string& value) -> int64_t
      {
        return *reinterpret_cast<const int64_t*>(value.data());
      }
    };

    struct Bool
    {
      static constexpr e_field_type field_type = zTYPE_BOOL;
      using type = bool;

      static __forceinline auto get(const std::string& value) -> bool
      {
        size_t bytes_read;
        return !!utils::read_varuint32(value.data(), bytes_read);
      }
    };

    struct String
    {
      static constexpr e_field_type field_type = zTYPE_STRING;
      using type = std::string;

      static __forceinline auto get(const std::string& value) -> std::string
      {
        size_t bytes_read;
        const auto length = utils::read_varuint32(value.data(), bytes_read);
        auto result = std::string{value.data() + bytes_read, length};
        return std::move(result);
      }
    };

    struct Group : String
    {
      static constexpr e_field_type field_type = zTYPE_GROUP;
    };

    struct Message : String
    {
      static constexpr e_field_type field_type = zTYPE_MESSAGE;
    };

    struct Bytes : String
    {
      static constexpr e_field_type field_type = zTYPE_BYTES;
    };

    struct Enum : Int32
    {
      static constexpr e_field_type field_type = zTYPE_ENUM;
    };

    struct Sfixed32 : Fixed32
    {
      static constexpr e_field_type field_type = zTYPE_SFIXED32;
    };

    struct Sfixed64 : Fixed64
    {
      static constexpr e_field_type field_type = zTYPE_SFIXED64;
    };

    struct Sint32
    {
      static constexpr e_field_type field_type = zTYPE_SINT32;
      using type = int32_t;
    };

    struct Sint64
    {
      static constexpr e_field_type field_type = zTYPE_SINT64;
      using type = int64_t;
    };
  } // types

  namespace base
  {
    struct field_vector
    {
      using Entry = std::pair<std::string, std::string>; // value_bytes, tag_and_value_bytes
      using Field = std::vector<Entry>;
      using Fields = std::vector<Field>;

      const std::shared_ptr<Fields> fields;
      const size_t max_size;

      explicit field_vector(const size_t max_size) : fields {std::make_shared<Fields>()}, max_size {max_size + 1}
      {
        fields->resize(this->max_size);
      }

      auto clear(const uint32_t field) const -> void
      {
        if (field >= max_size)
          throw std::runtime_error("field range error");

        auto& fld = fields->at(field);

        if (!fld.empty())
          fld.clear();
      }

      auto has(const uint32_t field, const size_t index = 0) const -> bool
      {
        if (field >= max_size)
          throw std::runtime_error("field range error");

        auto& fld = fields->at(field);

        return fld.size() > index;
      }

      auto count(const uint32_t field) const -> size_t
      {
        if (field >= max_size)
          throw std::runtime_error("field range error");

        auto& fld = fields->at(field);

        return fld.size();
      }

      auto add(const uint32_t field, const Entry& entry) const -> void
      {
        if (field >= max_size)
          throw std::runtime_error("field range error");

        fields->at(field).emplace_back(entry);
      }

      auto set(const uint32_t field, const Entry& entry, const size_t index = 0) const -> void
      {
        if (field >= max_size)
          throw std::runtime_error("field range error");

        auto& fld = fields->at(field);

        if (index == 0 && fld.empty())
        {
          fld.emplace_back(entry);
        }
        else
        {
          if (index >= fld.size())
            throw std::runtime_error("field range error");

          fld.at(index) = entry;
        }
      }

      auto get(const uint32_t field, const size_t index = 0) const -> std::string
      {
        if (field >= max_size)
          throw std::runtime_error("field range error");

        const auto& fld = fields->at(field);

        if (index >= fld.size())
          throw std::runtime_error("field range error");

        return fld.at(index).first;
      }

      auto get_all(const uint32_t field) const -> std::vector<std::string>
      {
        if (field >= max_size)
          throw std::runtime_error("field range error");

        const auto& fld = fields->at(field);

        std::vector<std::string> ret;

        for (const auto& v : fld)
          ret.emplace_back(v.first);

        return std::move(ret);
      }
    };

    template <int Field, typename Type>
    struct header
    {
      static constexpr uint32_t field = Field;
      static constexpr e_field_type type = Type::field_type;
      static constexpr e_wire_type wire_type = k_wire_type_for_field_type[type];
    };

    struct msg
    {
      /*
       * For allow custom types :)
       */
      static constexpr e_field_type field_type = zTYPE_STRING;
      using type = std::string;
      //

      virtual ~msg() = default;
      msg() = delete;

      explicit msg(const size_t max_size) : fv_ {field_vector(max_size)}
      {
      }

      /*
       * @brief Serialize internal container in to buffer
       *
       * @return std::string
       */
      auto serialize() const -> std::string
      {
        std::string result;
        for (const auto& f0 : *fv_.fields)
          for (const auto& f1 : f0)
            result.append(f1.second);

        return std::move(result);
      }

      /*
       * @brief Parse source buffer
       */
      auto parse(const uint8_t* buf, size_t bytes) -> void
      {
        if (buf == nullptr)
          return;

        size_t pos = 0, bytes_read;
        uint32_t field, wire_type;
        std::string field_value_bytes, field_full_bytes;

        while (pos < bytes)
        {
          std::tie(field, wire_type, field_value_bytes, field_full_bytes) = utils::read_field(buf + pos, bytes_read);

          if (bytes_read == 0)
            break;

          if (field >= fv_.max_size)
            throw std::runtime_error("field range error");

          fv_.fields->at(field).emplace_back(field_value_bytes, field_full_bytes);
          pos += bytes_read;
        }
      }

    protected:
      const field_vector fv_;
    };

    template <int Field, typename Type>
    struct field
    {
      virtual ~field() = default;

      explicit field(const field_vector& fv) : fv_ {fv}
      {
      }

      /*
       * @brief Clear all values for field
       */
      auto clear() const -> void
      {
        fv_.clear(hdr.field);
      }

      /*
       * @brief Check if value exist in field
       * @param index=0 - Where the value will be checked
       */
      auto has(size_t index = 0) const -> bool
      {
        return fv_.has(hdr.field, index);
      }

      /*
       * @brief Get count all values for field
       * @return count values in field
       */
      auto count() const -> size_t
      {
        return fv_.count(hdr.field);
      }

    protected:
      static constexpr header<Field, Type> hdr{};
      const field_vector fv_;
    };
  } // base


  template <int Field, typename Type, typename S = void>
  struct pbfield;


  /*****************/
  /**DEFAULT TYPES**/
  /*****************/
  template <int Field, typename Type>
  struct pbfield<Field, Type, typename std::enable_if<!std::is_base_of<base::msg, Type>::value>::type
    > : base::field<Field, Type>
  {
    virtual ~pbfield() = default;

    explicit pbfield(const base::field_vector& fv) : base::field<Field, Type> {fv}
    {
    }

    using type = typename Type::type;

    /*
     * @brief Add value for repeatable field
     */
    auto add(type&& value) const -> void
    {
      const auto pair = utils::get_field_bytes(hdr.field, hdr.wire_type, value);
      fv_.add(hdr.field, pair);
    }

    /*
     * @brief Set value for field
     * @param value
     * @param index=0 - Where the value will be changed
     */
    auto set(type&& value, size_t index = 0) const -> void
    {
      const auto pair = utils::get_field_bytes(hdr.field, hdr.wire_type, value);
      fv_.set(hdr.field, pair, index);
    }

    /*
     * @brief Set value for field
     * @param value
     * @param index=0 - Where the value will be changed
     */
    auto set(const type& value, size_t index = 0) const -> void
    {
      const auto pair = utils::get_field_bytes(hdr.field, hdr.wire_type, value);
      fv_.set(hdr.field, pair, index);
    }

    /*
     * @brief Set value for field
     * @param vector values
     */
    auto set(std::vector<type>&& values) const -> void
    {
      fv_.clear(hdr.field);

      for (auto& v : values)
      {
        const auto pair = utils::get_field_bytes(hdr.field, hdr.wire_type, v);
        fv_.add(hdr.field, pair);
      }
    }

    /*
     * @brief Set value for field
     * @param vector values
     */
    auto set(std::vector<type>& values) const -> void
    {
      fv_.clear(hdr.field);

      for (auto& v : values)
      {
        const auto pair = utils::get_field_bytes(hdr.field, hdr.wire_type, v);
        fv_.add(hdr.field, pair);
      }
    }

    /*
     * @brief Get COPY all values from field
     * @return collection std::vector<T>
     */
    auto get_all() const -> std::vector<type>
    {
      const auto values = fv_.get_all(hdr.field);

      std::vector<type> ret;

      for (const auto& v : values)
        ret.emplace_back(Type::get(v));

      return std::move(ret);
    }

    /*
     * @brief Get COPY value from field
     * @param index=0 Where the value will be getted
     * @return T
     */
    auto get(size_t index = 0) const -> type
    {
      const auto value = fv_.get(hdr.field, index);
      return Type::get(value);
    }

  private:
    using base::field<Field, Type>::hdr;
    using base::field<Field, Type>::fv_;
  };

  /****************/
  /**CUSTOM TYPES**/
  /****************/
  template <int Field, typename Type>
  struct pbfield<Field, Type, typename std::enable_if<std::is_base_of<base::msg, Type>::value>::type
    > : base::field<Field, Type>
  {
    virtual ~pbfield() = default;

    explicit pbfield(const base::field_vector& fv) : base::field<Field, Type> {fv}
    {
    }

    using type = typename Type::type;

    /*
     * @brief Add value for repeatable field
     */
    auto add(const Type& value) const -> void
    {
      const auto v = value.serialize();
      const auto pair = utils::get_field_bytes(hdr.field, hdr.wire_type, v);
      fv_.add(hdr.field, pair);
    }

    /*
     * @brief Set value for field
     * @param value
     * @param index=0 - Where the value will be changed
     */
    auto set(const Type& value, size_t index = 0) const -> void
    {
      const auto v = value.serialize();
      const auto pair = utils::get_field_bytes(hdr.field, hdr.wire_type, v);
      fv_.set(hdr.field, pair, index);
    }

    /*
     * @brief Get COPY all values from field
     * @return collection std::vector< T >
     */
    auto get_all() const -> std::vector<Type>
    {
      const auto values = fv_.get_all(hdr.field);

      std::vector<Type> ret;

      for (const auto& v : values)
      {
        const auto s = types::String::get(v);
        ret.emplace_back(s);
      }

      return std::move(ret);
    }

    /*
     * @brief Get COPY value from field
     * @param index=0 Where the value will be getted
     * @return T
     */
    auto get(size_t index = 0) const -> Type
    {
      const auto value = fv_.get(hdr.field, index);
      const auto s = types::String::get(value);
      return std::move(Type(s));
    }

  private:
    using base::field<Field, Type>::hdr;
    using base::field<Field, Type>::fv_;
  };

  template <size_t MsgSize>
  struct pbmsg : base::msg
  {
    virtual ~pbmsg() = default;

    explicit pbmsg() : msg(MsgSize)
    {
    }

    explicit pbmsg(void* buf, size_t bytes) : msg(MsgSize)
    {
      parse(reinterpret_cast<const uint8_t*>(buf), bytes);
    }

    explicit pbmsg(const std::string& buf) : msg(MsgSize)
    {
      parse(reinterpret_cast<const uint8_t*>(buf.data()), buf.size());
    }
  };
} // pbwrap
