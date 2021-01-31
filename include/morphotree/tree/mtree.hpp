#pragma once

#include "morphotree/core/alias.hpp"
#include "morphotree/core/box.hpp"
#include <memory>
#include <list>
#include <vector>
#include <limits>

#include "morphotree/tree/ct_builder.hpp"
#include "morphotree/adjacency/adjacency.hpp"
#include "morphotree/core/sort.hpp"

#include <queue>

namespace morphotree
{
  template<class WeightType>
  class MTNode
  {
  public:
    using ValueType = WeightType; 
    using NodePtr = std::shared_ptr<MTNode<WeightType>>;

    MTNode(uint id=0);

    inline uint id() const { return id_; }
    inline uint& id() { return id_; }
    inline void id(uint newid) { id_ = newid; } 

    inline WeightType& level() { return level_; }
    inline WeightType  level() const { return level_; }
    inline void level(WeightType v) { level_ = v; }

    inline const std::vector<uint32>& cnps() const { return cnps_; }
    inline void appendCNP(uint32 cnp) { cnps_.push_back(cnp); }
    inline void includeCNPS(const std::vector<uint32> &cnps);

    inline NodePtr parent() { return parent_; }
    inline const NodePtr parent() const  { return parent_; }
    inline void parent(NodePtr parent) { parent_ = parent;}

    inline void appendChild(std::shared_ptr<MTNode> child) { children_.push_back(child); }
    inline void includeChildren(const std::vector<NodePtr> &children);
    inline void removeChild(NodePtr c) { children_.remove(c); }
    inline const std::list<NodePtr>& children() const { return children_; }
    
    std::vector<uint32> reconstruct() const;
    void reconstruct(std::vector<uint32> &pixels, const NodePtr node) const; 
    std::vector<bool> reconstruct(const Box &domain) const;

    NodePtr copy() const;

  private:
    uint32 id_;
    WeightType level_;
    std::vector<uint32> cnps_;
    NodePtr parent_;
    std::list<NodePtr> children_;
  };

  template<class WeightType>
  class MorphologicalTree
  {
  public:
    using NodePtr = typename MTNode<WeightType>::NodePtr; 
    using NodeType = MTNode<WeightType>;

    MorphologicalTree(const std::vector<WeightType> &f, const CTBuilderResult &res);
    MorphologicalTree(std::vector<uint32> &&cmap, std::vector<NodePtr> &&nodes);
    MorphologicalTree();

    const NodePtr node(uint id) const { return nodes_[id]; }
    NodePtr node(uint id) { return nodes_[id]; }
    
    const NodePtr root() const { return root_; }
    NodePtr root() { return root_; }

    inline std::vector<uint32> reconstructNode(uint32 nodeId) const { return nodes_[nodeId]->reconstruct(); }
    std::vector<bool> reconstructNode(uint32 nodeId, const Box &domain) const { return nodes_[nodeId]->reconstruct(domain); };

    uint32 numberOfNodes() const { return nodes_.size(); }

    uint32 numberOfCNPs() const { return cmap_.size(); }

    void tranverse(std::function<void(const NodePtr node)> visit) const;

    std::vector<WeightType> reconstructImage() const;
    
    void idirectFilter(std::function<bool(const NodePtr)> keep);

    MorphologicalTree<WeightType> directFilter(std::function<bool(const NodePtr)> keep) const;

    void traverseByLevel(std::function<void(const NodePtr)> visit) const;

    void traverseByLevel(std::function<void(NodePtr)> visit);

    inline NodePtr smallComponent(uint32 idx) { return nodes_[cmap_[idx]]; }

    MorphologicalTree<WeightType> copy() const;

  private:
    void performDirectFilter(MorphologicalTree<WeightType> &tree, std::function<bool(const NodePtr)> keep) const;

  private:
    std::vector<NodePtr> nodes_;
    std::vector<uint32> cmap_;
    NodePtr root_;
  };

  template<class WeightType> 
  MorphologicalTree<WeightType> buildMaxTree(const std::vector<WeightType> &f,
    std::shared_ptr<Adjacency> adj);

  template<class WeightType>
  MorphologicalTree<WeightType> buildMinTree(const std::vector<WeightType> &f,
    std::shared_ptr<Adjacency> adj);


  // ======================[ IMPLEMENTATION ] ===================================================================
  template<class WeightType>
  MTNode<WeightType>::MTNode(uint id)
    :id_{id}, level_{0}, parent_{nullptr}
  {}

  template<class WeightType>
  std::vector<uint32> MTNode<WeightType>::reconstruct() const
  {
    std::vector<uint32> pixels{cnps_};
    for (NodePtr child : children_) {
      reconstruct(pixels, child);
    }
    return pixels;
  }

  template<class WeightType>
  void MTNode<WeightType>::reconstruct(std::vector<uint32> &pixels, const NodePtr node) const
  {
    pixels.insert(pixels.end(), node->cnps().begin(), node->cnps().end());
    for (NodePtr child: node->children()) {
      reconstruct(pixels, child);
    }
  }

  template<class WeightType>
  std::vector<bool> MTNode<WeightType>::reconstruct(const Box &domain) const
  {
    std::vector<uint32> indices = reconstruct();
    std::vector<bool> img(domain.numberOfPoints(), false);
    for (uint32 i : indices) {
      img[i] = true;
    }
    return img;
  }

  template<class WeightType>
  void MTNode<WeightType>::includeCNPS(const std::vector<uint32>& cnps)
  {
    cnps_.insert(cnps_.end(), cnps.begin(), cnps.end());
  }

  template<class WeightType>
  void MTNode<WeightType>::includeChildren(const std::vector<NodePtr> &children)
  {
    children_.insert(children_.end(), children.begin(), children.end());
  }

  template<class WeightType>
  typename MTNode<WeightType>::NodePtr MTNode<WeightType>::copy() const
  {
    NodePtr cnode = std::make_shared<MTNode<WeightType>>(id_);
    cnode->level(level_);  
    cnode->cnps_ = cnps_;
    return cnode;
  }

  // ========================== [TREEE] =========================================================================
  template<class WeightType>
  MorphologicalTree<WeightType>::MorphologicalTree()
  { }

  template<class WeightType>
  MorphologicalTree<WeightType>::MorphologicalTree(std::vector<uint32> &&cmap, std::vector<NodePtr> &&nodes)
    :cmap_{cmap}, nodes_{nodes}
  {
    root_ = nodes_[0];
  }

  template<class WeightType>
  MorphologicalTree<WeightType>::MorphologicalTree(const std::vector<WeightType> &f, 
    const CTBuilderResult &res)
  {
    const uint32 UNDEF = std::numeric_limits<uint32>::max();
    std::vector<uint32> sortedLevelRoots;
    cmap_.resize(f.size(), UNDEF);

    for (uint32 i = 0; i < res.R.size(); i++) {
      uint32 p = res.R[i];
      if (f[res.parent[p]] != f[p] || res.parent[p] == p)
        sortedLevelRoots.push_back(p);
    }

    nodes_.resize(sortedLevelRoots.size(), nullptr);

    uint32 p = sortedLevelRoots[sortedLevelRoots.size()-1];
    NodePtr root = std::make_shared<NodeType>(0);
    root->level(f[p]);
    root->appendCNP(p);
    root->parent(nullptr);
    cmap_[p] = root->id();
    nodes_[0] = root;
    root_ = root;

    
    for (uint32 i = 2; i <= sortedLevelRoots.size(); i++) {
      uint32 p = sortedLevelRoots[sortedLevelRoots.size()-i];
      cmap_[p] = i-1; 
      
      NodePtr node = std::make_shared<NodeType>(i-1);
      NodePtr parentNode = nodes_[cmap_[res.parent[p]]]; 
      node->id(i-1);
      node->parent(parentNode);
      node->level(f[p]);
      node->appendCNP(p);
      parentNode->appendChild(node);

      nodes_[i-1] = node; 
      
    }

    for (uint32 i = 0; i < f.size(); i++) {
      if (cmap_[i] == UNDEF) {
        cmap_[i] = cmap_[res.parent[i]];
        nodes_[cmap_[i]]->appendCNP(i);
      }
    }
  }
  
  template<class WeightType>
  void MorphologicalTree<WeightType>::tranverse(std::function<void(const NodePtr node)> visit) const 
  {
    for(uint32 i = 1; i <= nodes_.size(); i++)  {
      visit(nodes_[nodes_.size() - i]);
    }
  }

  template<class WeightType> 
  MorphologicalTree<WeightType> buildMaxTree(const std::vector<WeightType> &f,
    std::shared_ptr<Adjacency> adj)
  {
    CTBuilder<WeightType> builder;
    std::vector<uint32> R = sortIncreasing(f);
    return MorphologicalTree<WeightType>(f, builder.build(f, adj, R));
  }

  template<class WeightType>
  MorphologicalTree<WeightType> buildMinTree(const std::vector<WeightType> &f,
    std::shared_ptr<Adjacency> adj)
  {
    CTBuilder<WeightType> builder;
    std::vector<uint32> R = sortDecreasing(f);
    return MorphologicalTree<WeightType>(f, builder.build(f, adj, R));
  }

  template<typename WeightType>
  std::vector<WeightType> MorphologicalTree<WeightType>::reconstructImage() const
  {
    std::vector<WeightType> f(cmap_.size());
    for (NodePtr node : nodes_) {
      for (uint idx : node->cnps()) {
        f[idx] = node->level();
      }
    }
    return f;
  }
  
  template<class WeightType>
  void MorphologicalTree<WeightType>::idirectFilter(std::function<bool(const NodePtr)> keep)
  {
    performDirectFilter(*this, keep);
  }

  template<class WeightType>
  MorphologicalTree<WeightType> MorphologicalTree<WeightType>::directFilter(
    std::function<bool(const NodePtr)> keep) const
  {
    MorphologicalTree<WeightType> ctree = copy();
    performDirectFilter(ctree, keep);
    return ctree;
  }

  template<typename WeightType>
  void MorphologicalTree<WeightType>::performDirectFilter(MorphologicalTree<WeightType> &tree, 
    std::function<bool(const NodePtr)> keep) const 
  {
    // remove node from data structure.
    uint32 numRemovedNodes = 0;
      
    std::queue<NodePtr> queue;
    queue.push(tree.root());

    while (!queue.empty())
    {
      NodePtr node = queue.front();
      queue.pop();
      
      for (NodePtr c : node->children()) {
        queue.push(c);
      }

      if (!keep(node) && node->parent() != nullptr) {
        numRemovedNodes++;  
        NodePtr parentNode = node->parent();
        parentNode->includeCNPS(node->cnps());
        parentNode->removeChild(node);
        for (NodePtr c : node->children()) {
          parentNode->appendChild(c);
          c->parent(parentNode);
        }
      }
    }

    // fix cmap and nodes arrays
    uint32 prevNumOfNodes = tree.numberOfNodes();
    tree.nodes_.clear();
    tree.nodes_.resize(prevNumOfNodes - numRemovedNodes);
    uint32 newId = 0;
    tree.traverseByLevel([&tree, &newId](NodePtr n) {
      n->id(newId);
      tree.nodes_[newId] = n;
      for (uint32 idx : n->cnps()) {
        tree.cmap_[idx] = newId;
      }
      newId++;
    }); 
  }


  template<typename WeightType>
  void MorphologicalTree<WeightType>::traverseByLevel(std::function<void(const NodePtr)> visit) const
  {
    std::queue<NodePtr> queue;
    queue.push(root());

    while (!queue.empty())
    {
      NodePtr node = queue.front();
      queue.pop();
      visit(node);
      for (NodePtr c : node->children()) {
        queue.push(c);
      }
    }
  }

  template<typename WeightType>
  void MorphologicalTree<WeightType>::traverseByLevel(std::function<void(NodePtr)> visit)
  {
    std::queue<NodePtr> queue;
    queue.push(root());

    while (!queue.empty())
    {
      NodePtr node = queue.front();
      queue.pop();
      visit(node);
      for (NodePtr c : node->children()) {
        queue.push(c);
      }
    }
  }

  template<class WeightType>
  MorphologicalTree<WeightType> MorphologicalTree<WeightType>::copy() const
  {
    MorphologicalTree ctree;
    ctree.nodes_.reserve(numberOfNodes());
    ctree.cmap_ = cmap_;

    for (NodePtr node : nodes_) {
      ctree.nodes_.push_back(node->copy());
    }

    traverseByLevel([this, &ctree](NodePtr node) {
      NodePtr cnode = ctree.nodes_[node->id()];
      
      if (node->parent() != nullptr) {
        NodePtr cparent = ctree.nodes_[node->parent()->id()];
        cnode->parent(cparent);
        cparent->appendChild(cnode); 
      }
    });
    ctree.root_ = ctree.nodes_[0];
    return ctree;
  }
} 