/*
 * Copyright (C) 2009, Willow Garage, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the names of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ROSCPP_SERIALIZATION_H
#define ROSCPP_SERIALIZATION_H

#include "types.h"
#include "serialized_message.h"
#include "message_traits.h"
#include "builtin_message_traits.h"
#include "time.h"
#include "exception.h"
#include "macros.h"

#include <vector>

#include <boost/array.hpp>
#include <boost/call_traits.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/mpl/and.hpp>
#include <boost/mpl/or.hpp>
#include <boost/mpl/not.hpp>

#define ROS_NEW_SERIALIZATION_API 1

/**
 * \brief Declare your serializer to use an allinone member instead of requiring 3 different serialization
 * functions.
 *
 * The allinone method has the form:
\verbatim
template<typename Stream, typename T>
inline static void allinone(Stream& stream, T t)
{
  stream.next(t.a);
  stream.next(t.b);
  ...
}
\endverbatim
 *
 * The only guarantee given is that Stream::next(T) is defined.
 */
#define ROS_DECLARE_ALLINONE_SERIALIZER \
  template<typename Stream, typename T> \
  inline static void write(Stream& stream, const T& t) \
  { \
    allinone<Stream, const T&>(stream, t); \
  } \
  \
  template<typename Stream, typename T> \
  inline static void read(Stream& stream, T& t) \
  { \
    allinone<Stream, T&>(stream, t); \
  } \
  \
  template<typename Stream, typename T> \
  inline static void serializedLength(Stream& stream, const T& t) \
  { \
    allinone<Stream, const T&>(stream, t); \
  }

namespace ros
{
namespace serialization
{
namespace mt = message_traits;
namespace mpl = boost::mpl;

class StreamOverrunException : public ros::Exception
{
public:
  StreamOverrunException(const std::string& what)
  : Exception(what)
  {}
};

void throwStreamOverrun();

/**
 * \brief Templated serialization class.  Default implementation provides backwards compatibility with
 * old message types.
 *
 * Specializing the Serializer class is the only thing you need to do to get the ROS serialization system
 * to work with a type.
 */
template<typename T>
struct Serializer
{
  /**
   * \brief Write an object to the stream.  Normally the stream passed in here will be a ros::serialization::OStream
   */
  template<typename Stream>
  inline static void write(Stream& stream, typename boost::call_traits<T>::param_type t)
  {
    t.serialize(stream.getData(), 0);
  }

  /**
   * \brief Read an object from the stream.  Normally the stream passed in here will be a ros::serialization::IStream
   */
  template<typename Stream>
  inline static void read(Stream& stream, typename boost::call_traits<T>::reference t)
  {
    t.__serialized_length = stream.getLength();
    t.deserialize(stream.getData());
  }

  /**
   * \brief Determine the serialized length of an object.  Normally the stream passed in here will be a ros::serialization::LStream
   */
  template<typename Stream>
  inline static void serializedLength(Stream& stream, typename boost::call_traits<T>::param_type t)
  {
    stream.advance(t.serializationLength());
  }
};

/**
 * \brief Serialize an object.  Stream here should normally be a ros::serialization::OStream
 */
template<typename T, typename Stream>
inline void serialize(Stream& stream, const T& t)
{
  Serializer<T>::write(stream, t);
}

/**
 * \brief Deserialize an object.  Stream here should normally be a ros::serialization::IStream
 */
template<typename T, typename Stream>
inline void deserialize(Stream& stream, T& t)
{
  Serializer<T>::read(stream, t);
}

/**
 * \brief Determine the serialized length of an object.  Stream here should normally be a ros::serialization::LStream
 */
template<typename T, typename Stream>
inline void serializationLength(Stream& stream, const T& t)
{
  Serializer<T>::serializedLength(stream, t);
}

#define ROS_CREATE_SIMPLE_SERIALIZER(Type) \
  template<> struct Serializer<Type> \
  { \
    template<typename Stream> inline static void write(Stream& stream, const Type v) \
    { \
      *reinterpret_cast<Type*>(stream.advance(sizeof(v))) = v; \
    } \
    \
    template<typename Stream> inline static void read(Stream& stream, Type& v) \
    { \
      v = *reinterpret_cast<Type*>(stream.advance(sizeof(v))); \
    } \
    \
    template<typename Stream> inline static void serializedLength(Stream& stream, const Type t) \
    { \
      stream.advance(sizeof(Type)); \
    } \
  };

ROS_CREATE_SIMPLE_SERIALIZER(uint8_t);
ROS_CREATE_SIMPLE_SERIALIZER(int8_t);
ROS_CREATE_SIMPLE_SERIALIZER(uint16_t);
ROS_CREATE_SIMPLE_SERIALIZER(int16_t);
ROS_CREATE_SIMPLE_SERIALIZER(uint32_t);
ROS_CREATE_SIMPLE_SERIALIZER(int32_t);
ROS_CREATE_SIMPLE_SERIALIZER(uint64_t);
ROS_CREATE_SIMPLE_SERIALIZER(int64_t);
ROS_CREATE_SIMPLE_SERIALIZER(float);
ROS_CREATE_SIMPLE_SERIALIZER(double);

/**
 * \brief Serializer specialized for bool (serialized as uint8)
 */
template<> struct Serializer<bool>
{
  template<typename Stream> inline static void write(Stream& stream, const bool v)
  {
    uint8_t b = (uint8_t)v;
    *reinterpret_cast<uint8_t*>(stream.advance(1)) = b;
  }

  template<typename Stream> inline static void read(Stream& stream, bool& v)
  {
    uint8_t b = *reinterpret_cast<uint8_t*>(stream.advance(1));
    v = (bool)b;
  }

  template<typename Stream> inline static void serializedLength(Stream& stream, const bool t)
  {
    stream.advance(1);
  }
};

/**
 * \brief  Serializer specialized for std::string
 */
template<template<typename T> class Allocator >
struct Serializer<std::basic_string<char, std::char_traits<char>, Allocator<char> > >
{
  typedef std::basic_string<char, std::char_traits<char>, Allocator<char> > StringType;

  template<typename Stream>
  inline static void write(Stream& stream, const StringType& str)
  {
    size_t len = str.size();
    stream.next((uint32_t)len);

    if (len > 0)
    {
      memcpy(stream.advance(len), str.data(), len);
    }
  }

  template<typename Stream>
  inline static void read(Stream& stream, StringType& str)
  {
    uint32_t len;
    stream.next(len);
    if (len > 0)
    {
      str = StringType((char*)stream.advance(len), len);
    }
    else
    {
      str.clear();
    }
  }

  template<typename Stream>
  inline static void serializedLength(Stream& stream, const StringType& str)
  {
    stream.advance(4 + (uint32_t)str.size());
  }
};

/**
 * \brief Serializer specialized for ros::Time
 */
template<>
struct Serializer<ros::Time>
{
  template<typename Stream>
  inline static void write(Stream& stream, const ros::Time& v)
  {
    stream.next(v.sec);
    stream.next(v.nsec);
  }

  template<typename Stream>
  inline static void read(Stream& stream, ros::Time& v)
  {
    stream.next(v.sec);
    stream.next(v.nsec);
  }

  template<typename Stream>
  inline static void serializedLength(Stream& stream, const ros::Time& v)
  {
    stream.advance(8);
  }
};

/**
 * \brief Serializer specialized for ros::Duration
 */
template<>
struct Serializer<ros::Duration>
{
  template<typename Stream>
  inline static void write(Stream& stream, const ros::Duration& v)
  {
    stream.next(v.sec);
    stream.next(v.nsec);
  }

  template<typename Stream>
  inline static void read(Stream& stream, ros::Duration& v)
  {
    stream.next(v.sec);
    stream.next(v.nsec);
  }

  template<typename Stream>
  inline static void serializedLength(Stream& stream, const ros::Duration& v)
  {
    stream.advance(8);
  }
};

/**
 * \brief Vector serializer.  Default implementation does nothing
 */
template<typename T, template<typename T> class Allocator, class Enabled = void>
struct VectorSerializer
{};

/**
 * \brief Vector serializer, specialized for non-fixed-size, non-simple types
 */
template<typename T, template<typename T> class Allocator>
struct VectorSerializer<T, Allocator, typename boost::disable_if<mt::IsFixedSize<T> >::type >
{
  typedef std::vector<T, Allocator<T> > VecType;
  typedef typename VecType::iterator IteratorType;
  typedef typename VecType::const_iterator ConstIteratorType;

  template<typename Stream>
  inline static void write(Stream& stream, const VecType& v)
  {
    stream.next((uint32_t)v.size());
    ConstIteratorType it = v.begin();
    ConstIteratorType end = v.end();
    for (; it != end; ++it)
    {
      stream.next(*it);
    }
  }

  template<typename Stream>
  inline static void read(Stream& stream, VecType& v)
  {
    uint32_t len;
    stream.next(len);
    v.resize(len);
    IteratorType it = v.begin();
    IteratorType end = v.end();
    for (; it != end; ++it)
    {
      stream.next(*it);
    }
  }

  template<typename Stream>
  inline static void serializedLength(Stream& stream, const VecType& v)
  {
    stream.advance(4);
    ConstIteratorType it = v.begin();
    ConstIteratorType end = v.end();
    for (; it != end; ++it)
    {
      stream.next(*it);
    }
  }
};

/**
 * \brief Vector serializer, specialized for fixed-size simple types
 */
template<typename T, template<typename T> class Allocator>
struct VectorSerializer<T, Allocator, typename boost::enable_if<mt::IsSimple<T> >::type >
{
  typedef std::vector<T, Allocator<T> > VecType;
  typedef typename VecType::iterator IteratorType;
  typedef typename VecType::const_iterator ConstIteratorType;

  template<typename Stream>
  inline static void write(Stream& stream, const VecType& v)
  {
    uint32_t len = (uint32_t)v.size();
    stream.next(len);
    if (!v.empty())
    {
      const uint32_t data_len = len * sizeof(T);
      memcpy(stream.advance(data_len), &v.front(), data_len);
    }
  }

  template<typename Stream>
  inline static void read(Stream& stream, VecType& v)
  {
    uint32_t len;
    stream.next(len);
    v.resize(len);

    if (len > 0)
    {
      const uint32_t data_len = sizeof(T) * len;
      memcpy(&v.front(), stream.advance(data_len), data_len);
    }
  }

  template<typename Stream>
  inline static void serializedLength(Stream& stream, const VecType& v)
  {
    stream.advance(4 + v.size() * sizeof(T));
  }
};

/**
 * \brief Vector serializer, specialized for fixed-size non-simple types
 */
template<typename T, template<typename T> class Allocator>
struct VectorSerializer<T, Allocator, typename boost::enable_if<mpl::and_<mt::IsFixedSize<T>, mpl::not_<mt::IsSimple<T> > > >::type >
{
  typedef std::vector<T, Allocator<T> > VecType;
  typedef typename VecType::iterator IteratorType;
  typedef typename VecType::const_iterator ConstIteratorType;

  template<typename Stream>
  inline static void write(Stream& stream, const VecType& v)
  {
    stream.next((uint32_t)v.size());
    ConstIteratorType it = v.begin();
    ConstIteratorType end = v.end();
    for (; it != end; ++it)
    {
      stream.next(*it);
    }
  }

  template<typename Stream>
  inline static void read(Stream& stream, VecType& v)
  {
    uint32_t len;
    stream.next(len);
    v.resize(len);
    IteratorType it = v.begin();
    IteratorType end = v.end();
    for (; it != end; ++it)
    {
      stream.next(*it);
    }
  }

  template<typename Stream>
  inline static void serializedLength(Stream& stream, const VecType& v)
  {
    stream.advance(4);
    if (!v.empty())
    {
      Stream s;
      s.next(v.front());
      stream.advance(s.getLength() * (uint32_t)v.size());
    }
  }
};

/**
 * \brief serialize version for std::vector
 */
template<typename T, template<typename T> class Allocator, typename Stream>
inline void serialize(Stream& stream, const std::vector<T, Allocator<T> >& t)
{
  VectorSerializer<T, Allocator>::write(stream, t);
}

/**
 * \brief deserialize version for std::vector
 */
template<typename T, template<typename T> class Allocator, typename Stream>
inline void deserialize(Stream& stream, std::vector<T, Allocator<T> >& t)
{
  VectorSerializer<T, Allocator>::read(stream, t);
}

/**
 * \brief serializationLength version for std::vector
 */
template<typename T, template<typename T> class Allocator, typename Stream>
inline void serializationLength(Stream& stream, const std::vector<T, Allocator<T> >& t)
{
  VectorSerializer<T, Allocator>::serializedLength(stream, t);
}

/**
 * \brief Array serializer, default implementation does nothing
 */
template<typename T, size_t N, class Enabled = void>
struct ArraySerializer
{};

/**
 * \brief Array serializer, specialized for non-fixed-size, non-simple types
 */
template<typename T, size_t N>
struct ArraySerializer<T, N, typename boost::disable_if<mt::IsFixedSize<T> >::type>
{
  typedef boost::array<T, N > ArrayType;
  typedef typename ArrayType::iterator IteratorType;
  typedef typename ArrayType::const_iterator ConstIteratorType;

  template<typename Stream>
  inline static void write(Stream& stream, const ArrayType& v)
  {
    ConstIteratorType it = v.begin();
    ConstIteratorType end = v.end();
    for (; it != end; ++it)
    {
      stream.next(*it);
    }
  }

  template<typename Stream>
  inline static void read(Stream& stream, ArrayType& v)
  {
    IteratorType it = v.begin();
    IteratorType end = v.end();
    for (; it != end; ++it)
    {
      stream.next(*it);
    }
  }

  template<typename Stream>
  inline static void serializedLength(Stream& stream, const ArrayType& v)
  {
    ConstIteratorType it = v.begin();
    ConstIteratorType end = v.end();
    for (; it != end; ++it)
    {
      stream.next(*it);
    }
  }
};

/**
 * \brief Array serializer, specialized for fixed-size, simple types
 */
template<typename T, size_t N>
struct ArraySerializer<T, N, typename boost::enable_if<mt::IsSimple<T> >::type>
{
  typedef boost::array<T, N > ArrayType;
  typedef typename ArrayType::iterator IteratorType;
  typedef typename ArrayType::const_iterator ConstIteratorType;

  template<typename Stream>
  inline static void write(Stream& stream, const ArrayType& v)
  {
    const uint32_t data_len = N * sizeof(T);
    memcpy(stream.advance(data_len), &v.front(), data_len);
  }

  template<typename Stream>
  inline static void read(Stream& stream, ArrayType& v)
  {
    const uint32_t data_len = N * sizeof(T);
    memcpy(&v.front(), stream.advance(data_len), data_len);
  }

  template<typename Stream>
  inline static void serializedLength(Stream& stream, const ArrayType& v)
  {
    stream.advance(N * sizeof(T));
  }
};

/**
 * \brief Array serializer, specialized for fixed-size, non-simple types
 */
template<typename T, size_t N>
struct ArraySerializer<T, N, typename boost::enable_if<mpl::and_<mt::IsFixedSize<T>, mpl::not_<mt::IsSimple<T> > > >::type>
{
  typedef boost::array<T, N > ArrayType;
  typedef typename ArrayType::iterator IteratorType;
  typedef typename ArrayType::const_iterator ConstIteratorType;

  template<typename Stream>
  inline static void write(Stream& stream, const ArrayType& v)
  {
    ConstIteratorType it = v.begin();
    ConstIteratorType end = v.end();
    for (; it != end; ++it)
    {
      stream.next(*it);
    }
  }

  template<typename Stream>
  inline static void read(Stream& stream, ArrayType& v)
  {
    IteratorType it = v.begin();
    IteratorType end = v.end();
    for (; it != end; ++it)
    {
      stream.next(*it);
    }
  }

  template<typename Stream>
  inline static void serializedLength(Stream& stream, const ArrayType& v)
  {
    Stream s;
    s.next(v.front());
    stream.advance(s.getLength() * N);
  }
};

/**
 * \brief serialize version for boost::array
 */
template<typename T, size_t N, typename Stream>
inline void serialize(Stream& stream, const boost::array<T, N>& t)
{
  ArraySerializer<T, N>::write(stream, t);
}

/**
 * \brief deserialize version for boost::array
 */
template<typename T, size_t N, typename Stream>
inline void deserialize(Stream& stream, boost::array<T, N>& t)
{
  ArraySerializer<T, N>::read(stream, t);
}

/**
 * \brief serializationLength version for boost::array
 */
template<typename T, size_t N, typename Stream>
inline void serializationLength(Stream& stream, const boost::array<T, N>& t)
{
  ArraySerializer<T, N>::serializedLength(stream, t);
}

/**
 * \brief Enum
 */
namespace stream_types
{
enum StreamType
{
  Input,
  Output,
  Length
};
}
typedef stream_types::StreamType StreamType;

/**
 * \brief Stream base-class, provides common functionality for IStream and OStream
 */
struct Stream
{
  /*
   * \brief Returns a pointer to the current position of the stream
   */
  inline uint8_t* getData() { return data_; }
  /**
   * \brief Advances the stream, checking bounds, and returns a pointer to the position before it
   * was advanced.
   * \throws StreamOverrunException if len would take this stream past the end of its buffer
   */
  ROS_FORCE_INLINE uint8_t* advance(uint32_t len)
  {
    uint8_t* old_data = data_;
    data_ += len;
    if (data_ > end_)
    {
      // Throwing directly here causes a significant speed hit due to the extra code generated
      // for the throw statement
      throwStreamOverrun();
    }
    return old_data;
  }

  /**
   * \brief Returns the amount of space left in the stream
   */
  inline uint32_t getLength() { return (uint32_t)(end_ - data_); }

protected:
  Stream(uint8_t* _data, uint32_t _count)
  : data_(_data)
  , end_(_data + _count)
  {}

private:
  uint8_t* data_;
  uint8_t* end_;
};

/**
 * \brief Input stream
 */
struct IStream : public Stream
{
  static const StreamType stream_type = stream_types::Input;

  IStream(uint8_t* data, uint32_t count)
  : Stream(data, count)
  {}

  /**
   * \brief Deserialize an item from this input stream
   */
  template<typename T>
  ROS_FORCE_INLINE void next(T& t)
  {
    deserialize(*this, t);
  }
};

/**
 * \brief Output stream
 */
struct OStream : public Stream
{
  static const StreamType stream_type = stream_types::Output;

  OStream(uint8_t* data, uint32_t count)
  : Stream(data, count)
  {}

  /**
   * \brief Serialize an item to this output stream
   */
  template<typename T>
  ROS_FORCE_INLINE void next(const T& t)
  {
    serialize(*this, t);
  }
};


/**
 * \brief Length stream
 *
 * LStream is not what you would normally think of as a stream, but it is used in order to support
 * allinone serializers.
 */
struct LStream
{
  static const StreamType stream_type = stream_types::Length;

  LStream()
  : count_(0)
  {}

  /**
   * \brief Add the length of an item to this length stream
   */
  template<typename T>
  ROS_FORCE_INLINE void next(const T& t)
  {
    serializationLength(*this, t);
  }

  /**
   * \brief increment the length by len
   */
  ROS_FORCE_INLINE uint8_t* advance(uint32_t len)
  {
    count_ += len;
    return 0;
  }

  /**
   * \brief Get the total length of this tream
   */
  inline uint32_t getLength() { return count_; }

private:
  uint32_t count_;
};

/**
 * \brief Version of serializationLength that does not require you to manually construct and use an LStream.  Returns the length.
 */
template<typename T>
inline uint32_t serializationLength(const T& t)
{
  LStream stream;
  Serializer<T>::serializedLength(stream, t);
  return stream.getLength();
}

/**
 * \brief Version of serializationLength that does not require you to manually construct and use an LStream.  Returns the length.  Specialized for boost::array
 */
template<typename T, size_t N>
inline uint32_t serializationLength(const boost::array<T, N>& t)
{
  LStream stream;
  ArraySerializer<T, N>::serializedLength(stream, t);
  return stream.getLength();
}

/**
 * \brief Version of serializationLength that does not require you to manually construct and use an LStream.  Returns the length.  Specialized for std::vector
 */
template<typename T, template<typename T> class Allocator>
inline uint32_t serializationLength(const std::vector<T, Allocator<T> >& t)
{
  LStream stream;
  VectorSerializer<T, Allocator>::serializedLength(stream, t);
  return stream.getLength();
}

/**
 * \brief Serialize a message
 */
template<typename M>
inline SerializedMessage serializeMessage(const M& message)
{
  SerializedMessage m;
  LStream lstream;
  serializationLength(lstream, message);
  m.num_bytes = lstream.getLength() + 4;
  m.buf.reset(new uint8_t[m.num_bytes]);

  OStream s(m.buf.get(), (uint32_t)m.num_bytes);
  serialize(s, (uint32_t)m.num_bytes - 4);
  m.message_start = s.getData();
  serialize(s, message);

  return m;
}

/**
 * \brief Serialize a service response
 */
template<typename M>
inline SerializedMessage serializeServiceResponse(bool ok, const M& message)
{
  SerializedMessage m;

  if (ok)
  {
    LStream lstream;
    serializationLength(lstream, message);
    m.num_bytes = lstream.getLength() + 5;
    m.buf.reset(new uint8_t[m.num_bytes]);

    OStream s(m.buf.get(), (uint32_t)m.num_bytes);
    serialize(s, (uint8_t)ok);
    serialize(s, (uint32_t)m.num_bytes - 5);
    serialize(s, message);
  }
  else
  {
    m.num_bytes = 5;
    m.buf.reset(new uint8_t[5]);
    OStream s(m.buf.get(), (uint32_t)m.num_bytes);
    serialize(s, (uint8_t)ok);
    serialize(s, (uint32_t)0);
  }

  return m;
}

/**
 * \brief Deserialize a message.  If includes_length is true, skips the first 4 bytes
 */
template<typename M>
inline void deserializeMessage(const SerializedMessage& m, M& message, bool includes_length = false)
{
  if (includes_length)
  {
    IStream s(m.buf.get() + 4, m.num_bytes - 4);
    deserialize(s, message);
  }
  else
  {
    IStream s(m.buf.get(), m.num_bytes);
    deserialize(s, message);
  }
}

} // namespace serialization
} // namespace ros

#endif // ROSCPP_SERIALIZATION_H
