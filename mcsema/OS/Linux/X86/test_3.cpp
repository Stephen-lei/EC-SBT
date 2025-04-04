#include <type_traits>
class Person {
};


class Room {
public:
    void add_person(Person person)
    {
        // do stuff
    }

private:
    Person* people_in_room;
};




int main()
{
    Person* p = new Person();
    bool a=std::is_void<Person>::value;
    return 0;
}