#include <stdio.h>
#include <stdint.h>

#include <iostream>
#include <fstream>
#include <thread>
#include <vector>

#include <taskflow/taskflow.hpp>

int main()
{
	tf::Taskflow taskflow;
	tf::Executor executor;

	// std::vector<int> items{1, 2, 3, 4, 5, 6, 7, 8};

	// auto task = taskflow.for_each(
	// 	items.begin(), items.end(), [](int item)
	// 	{ std::cout << item << '\n'; });

	// taskflow.emplace([]()
	// 				 { std::cout << "\nS - Start\n"; })
	// 	.name("S")
	// 	.precede(task);
	// taskflow.emplace([]()
	// 				 { std::cout << "\nT - End\n"; })
	// 	.name("T")
	// 	.succeed(task);

	

	// tf::Executor executor;
	// executor.run(taskflow).wait();
	tf::Task A = taskflow.emplace([]() {}).name("A");
	tf::Task C = taskflow.emplace([]() {}).name("C");
	tf::Task D = taskflow.emplace([]() {}).name("D");

	tf::Task B = taskflow.emplace([](tf::Subflow &subflow)
								  {
		tf::Task B1 = subflow.emplace([]() {}).name("B1");
		tf::Task B2 = subflow.emplace([]() {}).name("B2");
		tf::Task B3 = subflow.emplace([]() {}).name("B3");
		B3.succeed(B1, B2); })
					 .name("B");

	A.precede(B, C); // A runs before B and C
	D.succeed(B, C); // D runs after  B and C

	 {
	 	std::ofstream os("taskflow.dot");
	 	taskflow.dump(os);
	 }
	executor.run(taskflow).wait();

	return 0;
}
