//
// Data Store
//
// Copyright 2000-2005 Matt T. Yourst <yourst@yourst.com>
//

#include <globals.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <datastore.h>

using namespace superstl;

DataStoreNode::DataStoreNode() {
  subnodes = null;
  parent = null;
  type = DS_NODE_TYPE_NULL;
  count = 0;
  name = null;
  summable = 0;
  histogramarray = 0;
  histomin = 0;
  histomax = 0;
  histostride = 0;
}

DataStoreNode::DataStoreNode(const char* name) {
  init(name, DS_NODE_TYPE_NULL, 0);
}

DataStoreNode::DataStoreNode(const char* name, NodeType type, int count) {
  init(name, type, count);
}

void DataStoreNode::init(const char* name, int type, int count) {
  this->type = type;
  this->name = strdup(name);
  this->count = count;
  subnodes = null;
  parent = null;
  summable = 0;
}

void DataStoreNode::cleanup() {
  if (type == DS_NODE_TYPE_STRING) {
    if (count > 1) {
      foreach (i, count) {
        delete this->values[i].s;
      }
      delete[] values;
    } else {
      delete this->value.s;
    }
  } else {
    if (count > 1)
      delete[] values;
  }
}

DataStoreNode::~DataStoreNode() {
  if (parent)
    assert(parent->remove(name));
  cleanup();
  removeall();
  if (subnodes)
    delete subnodes;
  delete name;
  subnodes = null;
  parent = null;
  name = null;
  type = DS_NODE_TYPE_NULL;
  count = 0;
}

DataStoreNode& DataStoreNode::add(DataStoreNode* node) {
  if (!subnodes) subnodes = new hash_t();
  node->parent = this;

  DataStoreNode** oldnode = subnodes->get(node->name);
  if (oldnode) {
    delete *oldnode;
  }

  subnodes->add(node->name, node);

  return *node;
}

bool DataStoreNode::remove(const char* key) {
  if (!subnodes)
    return false;
  return subnodes->remove(key);
}

void DataStoreNode::removeall() {
  DataStoreNodeDirectory& a = getentries();

  foreach (i, a.length) {
    delete a[i].value;
  }

  if (subnodes) assert(subnodes->count == 0);

  delete &a;
}

DataStoreNode::DataType DataStoreNode::getdata() const {
  if (!count) {
    DataType dummy;
    dummy.w = 0;
    return dummy;
  }

  return (count == 1) ? value : values[0];
}

DataStoreNode* DataStoreNode::search(const char* key) const {
  if (!subnodes)
    return null;
  DataStoreNode** nodeptr = (*subnodes)(key);
  return (nodeptr) ? *nodeptr : null;
}

DataStoreNode* DataStoreNode::searchpath(const char* path) const {
  char* pbase = strdup(path);
  const DataStoreNode* ds = this;

  char* p = pbase;

  p = strtok(p, "/");
  while (p) {
    DataStoreNode* dsn = ds->search(p);
    if (!dsn) {
      delete pbase;
      return null;
    }
    ds = dsn;
    p = strtok(null, "/");
  }

  delete pbase;
  return (DataStoreNode*)ds;
}

DataStoreNode& DataStoreNode::get(const char* key) {
  DataStoreNode* node = search(key);
  if (node)
    return *node;

  node = new DataStoreNode(key, DS_NODE_TYPE_NULL);
  add(node);
  return *node;
}

//
// Type: null
//

//
// Type: W64s (int)
//

DataStoreNode::DataStoreNode(const char* name, W64s value) {
  init(name, DS_NODE_TYPE_INT, 1);
  this->value.w = value;
}

DataStoreNode::DataStoreNode(const char* name, const W64s* values, int count, bool histogram) {
  init(name, DS_NODE_TYPE_INT, count);
  this->values = (count) ? (new DataType[count]) : null;
  if (this->values) arraycopy(this->values, (DataType*)values, count);
}


DataStoreNode& DataStoreNode::add(const char* key, W64s* value, int count, W64s histomin, W64s histomax, W64s histostride) {
  DataStoreNode* ds = new DataStoreNode(key, (W64s*)value, count);
  ds->histogramarray = 1;
  ds->histomin = histomin;
  ds->histomax = histomax;
  ds->histostride = histostride;

  return add(ds);
}

DataStoreNode& DataStoreNode::operator =(W64s data) {
  cleanup();
  this->type = DS_NODE_TYPE_INT;
  this->value.w = data;
  return *this;
}

DataStoreNode::operator W64s() const {
  switch (type) {
  case DS_NODE_TYPE_INT:
    return getdata().w; break;
  case DS_NODE_TYPE_FLOAT:
    return (W64s)getdata().f; break;
  case DS_NODE_TYPE_STRING:
    return atoll(getdata().s); break;
  case DS_NODE_TYPE_NULL:
    return 0;
  }
  return 0;
}

DataStoreNode::operator W64s*() const {
  assert(type == DS_NODE_TYPE_INT);
  return (!count) ? null : (count == 1) ? (W64s*)&value : (W64s*)values;
}

DataStoreNode::operator W64*() const {
  return (W64*)(W64s*)(*this);
}

//
// Type: double (float)
//

DataStoreNode::DataStoreNode(const char* name, double data) {
  init(name, DS_NODE_TYPE_FLOAT, 1);
  this->value.f = data;
}

DataStoreNode::DataStoreNode(const char* name, const double* values, int count) {
  init(name, DS_NODE_TYPE_FLOAT, count);
  this->values = (count) ? (new DataType[count]) : null;
  if (this->values) arraycopy(this->values, (DataType*)values, count);
}

DataStoreNode& DataStoreNode::operator =(double data) {
  cleanup();
  this->type = DS_NODE_TYPE_FLOAT;
  this->value.f = data;
  return *this;
}

DataStoreNode::operator double() const {
  switch (type) {
  case DS_NODE_TYPE_INT:
    return (W64s)getdata().w; break;
  case DS_NODE_TYPE_FLOAT:
    return getdata().f; break;
  case DS_NODE_TYPE_STRING:
    return atof(getdata().s); break;
  case DS_NODE_TYPE_NULL:
    return 0;
  }
  return 0;
}

DataStoreNode::operator double*() const {
  assert(type == DS_NODE_TYPE_FLOAT);
  return (!count) ? null : (count == 1) ? (double*)&value : (double*)values;
}

DataStoreNode::operator float() const {
  return (double)(*this);
}

//
// Type: const char* (string)
//

DataStoreNode::DataStoreNode(const char* name, const char* value) {
  init(name, DS_NODE_TYPE_STRING, 1);
  this->value.s = strdup(value);
}

DataStoreNode::DataStoreNode(const char* name, const char** values, int count) {
  init(name, DS_NODE_TYPE_FLOAT, count);
  this->values = (count) ? (new DataType[count]) : null;
  if (this->values) {
    foreach (i, count) {
      this->values[i].s = strdup(values[i]);
    }
  }
}

DataStoreNode& DataStoreNode::operator =(const char* data) {
  cleanup();
  this->type = DS_NODE_TYPE_FLOAT;
  this->value.s = strdup(data);
  return *this;
}

const char* DataStoreNode::string() const {
  assert(type == DS_NODE_TYPE_STRING);
  return getdata().s;
}

DataStoreNode::operator const char**() const {
  assert(type == DS_NODE_TYPE_STRING);
  return (!count) ? null : (count == 1) ? (const char**)&value : (const char**)values;
}

DataStoreNodeDirectory& DataStoreNode::getentries() const {
  return (subnodes) ? subnodes->getentries() : *(new DataStoreNodeDirectory());
}

DataStoreNode* DataStoreNode::subtract(DataStoreNode& prev) {
  DataStoreNode* newnode = null;

  assert(prev.type == type);
  assert(prev.count == count);

  switch (type) {
  case DS_NODE_TYPE_NULL: {
    newnode = new DataStoreNode(name);
    break;
  }
  case DS_NODE_TYPE_INT: {
    if (count == 1) {
      newnode = new DataStoreNode(name, (W64s)((W64s)(*this) - (W64s)prev));
    } else {
      W64s* a = new W64s[count];
      W64s* nodearray = (*this);
      W64s* prevarray = prev;

      foreach (i, count) {
        a[i] = nodearray[i] - prevarray[i];
      }

      newnode = new DataStoreNode(name, a, count);
    }
    break;
  }
  case DS_NODE_TYPE_FLOAT: {
    if (count == 1) {
      newnode = new DataStoreNode(name, (double)((double)(*this) - (double)prev));
    } else {
      double* a = new double[count];
      double* nodearray = (*this);
      double* prevarray = prev;

      foreach (i, count) {
        a[i] = nodearray[i] - prevarray[i];
      }

      newnode = new DataStoreNode(name, a, count);
    }
    break;
  }
  case DS_NODE_TYPE_STRING: {
    if (count == 1) {
      newnode = new DataStoreNode(name, prev.string());
    } else {
      newnode = new DataStoreNode(name, (const char**)prev, prev.count);
    }
    break;
  }
  default:
    assert(false);
  }

  DataStoreNodeDirectory& list = getentries();
  foreach (i, list.length) {
    DataStoreNode& subnode = *list[i].value;
    DataStoreNode& subprev = prev(list[i].key);

    newnode->add(subnode.subtract(subprev));
  }

  delete &list;

  return newnode;
}

double DataStoreNode::sum() const {
  double result = (double)(*this);

  DataStoreNodeDirectory& list = getentries();
  foreach (i, list.length) {
    result += list[i].value->sum();
  }

  delete &list;

  return result;
}

DataStoreNode& DataStoreNode::histogram(const W64* values, int count) {
  summable = 1;
  foreach (i, count) {
    stringbuf sb; sb << i;
    add(sb, values[i]);
  }
  return *this;
}

DataStoreNode& DataStoreNode::histogram(const char** names, const W64* values, int count) {
  summable = 1;
  foreach (i, count) {
    add(names[i], values[i]);
  }
  return *this;
}

static inline int digits(W64 v) {
  stringbuf sb;
  sb << v;
  return strlen(sb);
}

ostream& DataStoreNode::print(ostream& os, bool percents, int depth, double supersum) const {
  stringbuf padding;
  foreach (i, depth) { padding << "  "; }
  os << padding;

  double selfsum = sum();

  if (percents && supersum) {
    double percent = percent(selfsum, supersum);
    if (selfsum == supersum) os << "[ 100% ] "; else os << "[ ", floatstring(percent, 3, 0), "% ] ";
  }

  switch (type) {
  case DS_NODE_TYPE_NULL: {
    os << name;
    break;
  }
  case DS_NODE_TYPE_INT: {
    os << name;
    if (count == 1) {
      os << " = ", value.w, ";";
    } else {
      os << "[", count, "] = {";

      if (histogramarray) {
        os << endl;

        W64 total = 0;
        W64 maxvalue = 0;
        W64 minvalue = -1ULL;
        foreach (i, count) {
          total += values[i].w;
          minvalue = min((W64)values[i].w, minvalue);
          maxvalue = max((W64)values[i].w, maxvalue);
        }

        W64 thresh = max((W64)math::ceil((double)total / 1000.0), (W64)1);
        W64 base = histomin;
        int width = digits(max(histomin, histomax));
        int valuewidth = digits(maxvalue);
        int w = max(width, valuewidth);

        os << padding, "  ", "Range:   ", intstring(histomin, w), " ", intstring(histomax, w), endl;
        os << padding, "  ", "Stride:  ", intstring(histostride, w), endl;

        os << padding, "  ", "ValRange:", intstring(minvalue, w), " ", intstring(maxvalue, w), endl;
        os << padding, "  ", "Total:   ", intstring(total, w), endl;
        os << padding, "  ", "Thresh:  ", intstring(thresh, w), endl;

        foreach (i, count) {
          W64 value = (W64)values[i].w;

          if (value >= thresh) {
            double percent = ((double)value / (double)total) * 100.0;
            os << padding, "  [ ", floatstring(percent, 3, 0), "% ] ",
              intstring(base, w), " ", 
              intstring(base + (histostride-1), w), " ",
              intstring(values[i].w, w), endl;
          }

          base += histostride;
        }
        os << padding;
      } else {
        foreach (i, count) {
          os << values[i].w;
          if (i != (count-1)) os << ", ";
        }
      }
      os << "};";
    }
    break;
  }
  case DS_NODE_TYPE_FLOAT: {
    os << name;
    if (count == 1) {
      os << " = ", value.f, ";";
    } else {
      os << "[", count, "] = {";
      foreach (i, count) {
        os << values[i].f;
        if (i != (count-1)) os << ", ";
      }
      os << "};";
    }
    break;
  }
  case DS_NODE_TYPE_STRING: {
    os << name;
    if (count == 1) {
      os << " = \"", value.s, "\"", ";";
    } else {
      os << "[", count, "] = {";
      foreach (i, count) {
        os << "\"", values[i].f, "\"";
        if (i != (count-1)) os << ", ";
      }
      os << "};";
    }
    break;
  }
  default:
    assert(false);
  }

  if (subnodes) {
    if (summable) os << " (total ", (W64s)selfsum, ")";
    os << " {", endl;
    DataStoreNodeDirectory& a = getentries();
    foreach (i, a.length) {
      a[i].value->print(os, percents, depth + 1, (summable) ? selfsum : 0);
    }
    foreach (i, depth) { os << "  "; }
    os << "}";
    delete &a;
  }
  os << endl;
  return os;
}

struct DataStoreNodeArrayHeader {
  W32 count;
  W64 histomin;
  W64 histomax;
  W64 histostride;
};

struct DataStoreNodeHeader {
  W32 magic;
  byte type;
  byte namelength;
  W16 isarray:1, summable:1, histogramarray:1;
  W32 subcount;
  // (optional DataStoreNodeArrayInfo iff (isarray == 1)
  // (null-terminated name)
  // (count * sizeof(type) bytes)
  // (all subnodes)
};

DataStoreNode::DataStoreNode(idstream& is) {
  read(is);
}

#define DSN_MAGIC_VER_1 0x314e5344 // 'DSN1'

bool DataStoreNode::read(idstream& is) {
  DataStoreNodeHeader h;
  is >> h;

  // Multiple versions can be supported with different readers

  assert(is);

  if (h.magic != DSN_MAGIC_VER_1) {
    cerr << "DataStoreNode::read(): ERROR: stream does not have proper DSN version 1 header (0x", 
      hexstring(h.magic, 32), ") at offset ", ftell(is), endl, flush;
    return false;
  }

  DataStoreNodeArrayHeader ah;

  if (h.isarray) {
    is >> ah;
    count = ah.count;
    histomin = ah.histomin;
    histomax = ah.histomax;
    histostride = ah.histostride;
  }

  name = new char[h.namelength+1];
  is.read((char*)name, h.namelength+1);
  type = h.type;
  summable = h.summable;
  histogramarray = h.histogramarray;

  count = (h.isarray) ? ah.count : 1;
  subnodes = null;
  parent = null;

  switch (type) {
  case DS_NODE_TYPE_NULL: {
    break;
  }
  case DS_NODE_TYPE_INT: {
    if (count == 1) {
      is >> value.w;
    } else {
      values = new DataType[count];
      is.read(values, count * sizeof(DataType));
    }
    break;
  }
  case DS_NODE_TYPE_FLOAT: {
    if (count == 1) {
      is >> value.f;
    } else {
      values = new DataType[count];
      is.read(values, count * sizeof(DataType));
    }
    break;
  }
  case DS_NODE_TYPE_STRING: {
    if (count == 1) {
      byte len;
      is >> len;
      value.s = new char[len+1];
      is.read((char*)value.s, len+1);
    } else {
      values = new DataType[count];
      foreach (i, count) {
        byte len;
        is >> len;
        values[i].s = new char[len+1];
        is.read((char*)values[i].s, len+1);
      }
    }
    break;
  }
  default:
    assert(false);
  }

  foreach (i, h.subcount) {
    add(new DataStoreNode(is));
  }
  return is;
}

odstream& DataStoreNode::write(odstream& os) const {
  DataStoreNodeHeader h;
  DataStoreNodeArrayHeader ah;

  int namelen = strlen(name);
  assert(namelen < 256);

  h.magic = DSN_MAGIC_VER_1;
  h.type = type;
  h.namelength = (byte)namelen;
  h.histogramarray = histogramarray;
  h.summable = summable;

  h.isarray = (count > 1);
  if (count > 1) {
    ah.count = count;
    ah.histomin = histomin;
    ah.histomax = histomax;
    ah.histostride = histostride;
  }

  h.subcount = (subnodes) ? subnodes->count : 0;

  os << h;
  if (h.isarray) os << ah;

  os.write(name, h.namelength + 1);

  switch (type) {
  case DS_NODE_TYPE_NULL: {
    break;
  }
  case DS_NODE_TYPE_INT: {
    if (count == 1) {
      os << value.w;
    } else {
      os.write(values, count * sizeof(DataType));
    }
    break;
  }
  case DS_NODE_TYPE_FLOAT: {
    if (count == 1) {
      os << value.f;
    } else {
      os.write(values, count * sizeof(DataType));
    }
    break;
  }
  case DS_NODE_TYPE_STRING: {
    if (count == 1) {
      int len = strlen(value.s);
      assert(len < 256);     
      os << (byte)len;
      os.write(value.s, len+1);
    } else {
      foreach (i, count) {
        int len = strlen(values[i].s);
        assert(len < 256);
        os << (byte)len;
        os.write(values[i].s, len+1);
      }
    }
    break;
  }
  default:
    assert(false);
  }

  if (subnodes) {
    DataStoreNodeDirectory& a = getentries();
    foreach (i, a.length) {
      a[i].value->write(os);
    }
    delete &a;
  }
  return os;
}
