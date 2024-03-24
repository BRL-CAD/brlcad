# Rendering with Neural Intersection Functions

## Personal Information

**Name:** Jin Ke

**University:** Tongji University

**Email:**  rainy.autumn.fall@gmail.com

**Time zone:** GMT+8


## Project Information

### Project Title
Rendering with Neural Intersection Functions
### Brief Summary of Project
Ray tracing involves computationally expensive calculations and geometry with varying degrees of complexity. According to Fujieda et. al[1], a novel Neural Intersection Function can be used for ray intersection queries. This method has better efficiency while ensuring image quality.  The objective of this project is to implement and rigorously evaluate this Neural Intersection Function against the industry-standard bounding volume hierarchy (BVH) algorithm, providing a comprehensive comparative analysis of their respective performance characteristics.
### Previous work
The Appleseed Integration Project[2] introduced physically-based rendering (PBR) to solid geometry CAD modeling within BRL-CAD. This project enables the creation of custom plugins to execute specialized ray-intersection logic. The plugin is defined at: https://github.com/BRL-CAD/brlcad/blob/main/src/art/brlcadplugin.cpp. The "intersect" function within this plugin determines how to calculate the ray-intersection logic.
```cpp
bool
BrlcadObject::intersect(const asr::ShadingRay& ray) const
{
    struct application app;
    ...
    return (rt_shootray(&app) == 1);
}
``` 
Function "compute_local_bbox" determines how to calculate bounding box of object:
```cpp
asr::GAABB3
BrlcadObject::compute_local_bbox() const
{
    if (!this->rtip)
	return asr::GAABB3(asr::GVector3(0, 0, 0), asr::GVector3(0, 0, 0));
    if (this->rtip->needprep)
	rt_prep_parallel(this->rtip, 1);
    return asr::GAABB3(asr::GVector3(V3ARGS(min)), asr::GVector3(V3ARGS(max)));
}
```
Through Appleseed, BRL-CAD is capable of generating rendered images.

### Detailed project description
This project aims to utilize NIF to accelerate the ray tracing process. I am currently planning to complete the following four three of the content:
#### Parts 1 : Data Collection and Dataset Construction.
Due to the fact that the NIF method involves two networks, namely the outer network and the inner network, with the demarcation point being the Axis Aligned Bounding Box (AABB), I plan to utilize BrlcadObject::compute_local_bbox() to compute the object's collision box. Subsequently, I will employ Appleseed to perform a rendering where all light sources will be treated as input data for the outer network. After exporting the AOV (Arbitrary Output Variables) file, I will obtain ray-intersection data for all light sources and collision boxes. These will serve as output data for the outer network and input data for the inner-layer network. By employing the ray data obtained from the first rendering pass to render the objects once more, I will obtain output data for the inner-layer network.
In order to acquire as much data as possible, multiple renderings are necessary.
#### Parts 2 : Constructing the Neural Network Framework.
<font color="red">One potential approach is to utilize existing C++ frameworks such as Shogun or mlpack.</font>
<font color="blue">One potential approach is to implement NN from scratch using AMD C++ Heterogeneous-Compute Interface for Portability (HIP) [AMD21] in order to train and run inference on GPUs since the network is not complex. 
</font>

In [1],the outer network in NIF contains two hidden lay- ers with 64 nodes in each of them, whereas the inner network comprises three hidden layers with 48 nodes each. Every hidden layer is followed by a leaky ReLU activation function except for the last one. To meet user demands, we will incorporate the number of hidden layers and the number of nodes as parameters, catering to models of varying sizes. Additionally, we can consider making neural network hyperparameters adjustable, allowing users to create different networks and train them using their preferred methods.
#### Parts 3 Rendering Efficiency and Quality Testing
To assess the feasibility of neural network-accelerated ray tracing, testing will involve multiple models and can be broken down into the following points:
* Rendering Speed Comparison: Compare the rendering times between traditional ray tracing methods and neural network-accelerated ray tracing for each model.
* Image Quality Assessment: Evaluate the quality of rendered images produced by neural network-accelerated ray tracing compared to traditional methods.
* Scalability Testing: Assess the scalability of the neural network approach by increasing the complexity of the scenes or the size of the dataset. 

During the testing process, it's essential to continuously analyze and improve the neural network.   
### Improtance of the Project
The importance of the project lies in its potential to revolutionize the field of ray tracing by leveraging neural networks to significantly improve rendering efficiency while maintaining or even enhancing image quality
### Deliverables
* A new interface for:
```
bool BrlcadObject::intersectByNN(const asr::ShadingRay& ray) const
```
* test interface for this method, from speed and quality
* Comparison report between the NN interface and traditional methods, based on comprehensive testing with various models.
* Wiki page update with detailed information about Appleseed integration.

## Development Schedule
   - **Community Bonding Period**
      - Familiarizing with Appleseed's plugins
      - Fixing bugs in Appleseed
      - Ask for other cleaning works from the mentors, if required
   - **Week 1-2(27 May)** 
      - Export AOV files to obtain ray-intersection data for light sources and collision boxes.
      - Build database
   - **Week 3-4(10 June)**
      - Build outer NN
      - do some test for outer NN 
   - **Week 5(24 June)**
      - Build inner NN
      - do some test for inner NN 
   - **Week 6(1 July)** 
      - Merge two networks
   - **Week 7(8 July)**
      - test method
   - **Week 8(15 July)**
      - Integrate NN framework with existing rendering pipeline in BRL-CAD.
   - **Week 9(22 July)**
      - Organize code
   - **Week 10-11(29 July)**
      - Test more geometric models
      - Update NN
   - **Week 11-12(5 August)** 
      - Write report
      - write wiki
   - **Final week**:  Submit Final Evaluation and Code to the Melange Home


## My preparation for the Project
* I have successfully build brl-cad with appleseed from source code
* I am quite familiar with the project 'art'[3]
* I have found some errors when build brl-cad with appleseed version 1.7+ and I submmitted a pull request[4]

## Why BRL-CAD?
During a rewarding four-month internship at a geometry engine company, I honed my skills in b-rep modeling and developed a profound interest in computational mathematics. My passion lies at the intersection of computational geometry and artificial intelligence, which motivated my pursuit of this program.
## Why me?
I have previously interned at a company, so I believe I have decent C++ skills. In the AI field, I have participated in several projects, although most of them were done using Python. I begin with this book [Neural Networks and Deep Learning] and implemented most of the models inside.(http://neuralnetworksanddeeplearning.com/) and I have im
Here are some projects which I involved in:

[DDPM-cherry](https://github.com/Rainy-fall-end/DDPM-cherry)
I created a dataset of cherry blossoms and used DDPM to generate related data.

[welding-prediction](https://github.com/Rainy-fall-end/welding-prediction)
Using neural networks to predict welding deviations.

## Reference
[1]https://arxiv.org/abs/2306.07191
[2]https://brlcad.org/wiki/Appleseed
[3]https://github.com/BRL-CAD/brlcad/tree/main/src/art
[4]https://github.com/BRL-CAD/brlcad/pull/117