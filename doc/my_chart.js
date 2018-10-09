function chart(id,data,data_names,xlabel,ylabel){

var margin = {_myt: 20, _right: 20, _bottom: 30, _left: 40};

var  width = 960 - margin._left - margin._right;
var  height = 500 - margin._myt - margin._bottom;
//edited by ARUNBENNY

var x = d3.scale.linear()
    .range([0, width]);

var y = d3.scale.linear()
    .range([height, 0]);

var color = d3.scale.category10();

var xAxis = d3.svg.axis()
    .scale(x)
    .orient("bottom");

var yAxis = d3.svg.axis()
    .scale(y)
    .orient("left");

var svg = d3.select(id).append("svg")
    .attr("width", width + margin._left + margin._right)
    .attr("height", height + margin._myt + margin._bottom)
    .append("g")
    .attr("transform", "translate(" + margin._left + "," + margin._myt + ")");

x.domain(d3.extent(data, function(d) { return d[0] })).nice();
y.domain(d3.extent(data, function(d) { return d[1] })).nice();

svg.append("g")
  .attr("class", "x axis")
  .attr("transform", "translate(0," + height + ")")
  .call(xAxis)
  .append("text")
  .attr("class", "label")
  .attr("x", width)
  .attr("y", -6)
  .style("text-anchor", "end")
  .text(xlabel);

svg.append("g")
  .attr("class", "y axis")
  .call(yAxis)
  .append("text")
  .attr("class", "label")
  .attr("transform", "rotate(-90)")
  .attr("y", 6)
  .attr("dy", ".71em")
  .style("text-anchor", "end")
  .text(ylabel)

svg.selectAll(".dot")
  .data(data)
  .enter().append("circle")
  .attr("class", "dot")
  .attr("r", 3.5)
  .attr("cx", function(d) { return x(d[0]); })
  .attr("cy", function(d) { return y(d[1]); })
  .style("fill", function(d) { return color(d[2]); });

var legend = svg.selectAll(".legend")
  .data(color.domain())
  .enter().append("g")
  .attr("class", "legend")
  .attr("transform", function(d, i) { return "translate(0," + i * 20 + ")"; });

legend.append("rect")
  .attr("x", width - 18)
  .attr("width", 18)
  .attr("height", 18)
  .style("fill", color);

legend.append("text")
  .attr("x", width - 24)
  .attr("y", 9)
  .attr("dy", ".35em")
  .style("text-anchor", "end")
  .text(function(d) { return data_names[d]; });

}

