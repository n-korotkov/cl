for task in src/tasks/task*.cpp; do
    g++ -std=c++20 -o $(basename "$task" .cpp) -O3 -Isrc src/*.cpp "$task"
done
