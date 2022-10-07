#ifndef UTL_RANDOM_H
#define UTL_RANDOM_H

/*
 TRex team
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2017 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include<iostream>
#include<map>
#include<string>
#include<random>
#include<functional>
#include<memory>

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}


using ParamHash = std::map<std::string, std::string>;


class RandomFunctionParameter{
    public:
        static uint16_t percent; 
};

/* Abstract class.
 * All Random Functions 
 * inherits from this class
*/
class RandomFunctions
{
public:
    std::string name;
    ParamHash param;
    virtual bool getProbability() = 0;
    /**
     * Random Function constructor
     * 
     * @param name: name of the function
     * @param param: param for the function
     */
    RandomFunctions(std::string name, ParamHash param)
    {
        name = name;
        param.insert(param.begin(), param.end());
    }
};

class BernoulliDistribution : public RandomFunctions
{
public:
    BernoulliDistribution(ParamHash param = {{"p", "0.5"}}) : RandomFunctions("bernoulli_distribution", param) { }

    bool getProbability()
    {
        std::default_random_engine generator;
        double p = atof(this->param["p"].c_str());
        std::bernoulli_distribution distribution(p);
        return distribution(generator);
    }
};


/**
 * Random Function Factory 
 */
template <typename Ts>
class RandomFunctionFactory
{
public:
    static std::unique_ptr<RandomFunctions> getRandomFunction(std::string functionName, ParamHash functionParam)
    {
        static const std::function<std::unique_ptr<RandomFunctions>()> rf[] = {
            [functionParam](){ return make_unique<Ts>(functionParam); }};
        for (int i = 0; i < 1; i++)
        {
            if (rf[i]()->name == functionName)
            {
                return rf[i]();
            }
        }
        return NULL;
    }
};

/* Generates bool value with given probability */
bool generate_bool(ParamHash param={{}}, std::string functionName="");


#endif /* UTL_RANDOM_H */