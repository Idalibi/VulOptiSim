#include "pch.h"
#include "camera.h"

Camera::Camera(glm::vec3 position, glm::vec3 up, glm::vec3 direction, float movement_speed, float rotation_speed)
    : position(position), up(glm::normalize(up)), direction(glm::normalize(direction)), movement_speed(movement_speed), rotation_speed(rotation_speed)// Constructor
{

}

glm::mat4 Camera::get_view_matrix() const //Hoeft dit als variabele gereturnd worden?
{
    glm::mat4 view_matrix = glm::lookAt(position, position + direction, up);
    return view_matrix;
}

glm::vec3 Camera::get_position() const //Haalt een positie op
{
    return position;
}

void Camera::set_position(const glm::vec3& new_position) //Set een positie door middel van een referentie naar de originele waarde
{
    position = new_position;
}

void Camera::update_position(const glm::vec3& position_offset) //Past ook een positie aan net als set position
{
    position += position_offset;
}

glm::vec3 Camera::get_up() const //Haalt waarde up op
{
    return up;
}

void Camera::set_up(const glm::vec3& new_up) //set de waarde up (check wat normalize is)
{
    up = glm::normalize(new_up);
}

void Camera::update_up(const glm::mat4& transformation_matrix)  //update up (zelfde stijl als de position dus wellicht kan dit gecombineerd worden)
{
    up = transformation_matrix * glm::vec4(up, 1.0f);
    up = glm::normalize(up);
}

void Camera::move_forward(float distance) //Beweeg naar voren (check waar dit aangeroepen word) (dit zou allemaal 1 methode kunnen worden met een direction of een overwrite)
{
    // Project direction onto the horizontal plane (XZ-plane)
    glm::vec3 horizontal_direction = glm::normalize(glm::vec3(direction.x, 0.0f, direction.z));
    update_position(horizontal_direction * distance * movement_speed);
}

void Camera::move_backward(float distance)
{
    // Project direction onto the horizontal plane (XZ-plane)
    move_forward(-distance);
}

void Camera::move_right(float distance)
{
    // Calculate the side axis on the horizontal plane
    glm::vec3 side_axis = glm::normalize(glm::cross(glm::vec3(direction.x, 0.0f, direction.z), up));
    update_position(side_axis * distance * movement_speed);
}

void Camera::move_left(float distance)
{
    // Calculate the side axis on the horizontal plane
    move_right(-distance);
}

void Camera::move_up(float distance)
{
    update_position(glm::vec3(0.f, movement_speed * distance, 0.f));
}

void Camera::move_down(float distance)
{
    //Methode al aangepast
    move_up(-distance);
}

void Camera::rotate_left(float speed)// Draaien links.
{
    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), rotation_speed * speed * glm::radians(1.f), glm::vec3(0.0f, 1.0f, 0.0f));
    direction = rotation * glm::vec4(direction, 1.0f);

    //Recalculate right vector to ensure perpendicularity then recalculate the up vector
    glm::vec3 right = glm::normalize(glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
    up = glm::normalize(glm::cross(right, direction));
}

void Camera::rotate_right(float speed) //Exact hetzelfde als rotate left (alleen -speed) (dus dubbel verwijderen ter opschoning) (misschien extra atribuut toevoegen aan de methode (l/r))
{
    //Deze methode is al aangepast
    rotate_left(-speed);
}

glm::vec3 Camera::get_direction() const
{
    return direction;
}

void Camera::set_direction(const glm::vec3& new_direction)
{
    direction = glm::normalize(new_direction);

    //Recalculate right vector to ensure perpendicularity then recalculate the up vector
    glm::vec3 right = glm::normalize(glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
    up = glm::normalize(glm::cross(right, direction));
}

void Camera::update_direction(const glm::mat4& transformation_matrix)
{
    direction = transformation_matrix * glm::vec4(direction, 1.0f);
    direction = glm::normalize(direction);
}

void Camera::update_direction(const glm::vec2& offset) //Method override
{
    float yaw_offset = offset.x * rotation_speed; //Horisontaal draaien om de Z-as (links en rechts)
    float pitch_offset = -offset.y * rotation_speed; //Verticaal draaien om de Y-as (boven en onder)

    float pitch = glm::degrees(glm::asin(direction.y)); // een asinus naar degrees? check deze methodes.    
    pitch += pitch_offset;

    //Clamp pitch to avoid flipping
    if (pitch > 89.0f)
    {
        pitch = 89.0f;
    }

    if (pitch < -89.0f)
    {
        pitch = -89.0f;
    }

    //Calculate the yaw angle
    float yaw = glm::atan(direction.z, direction.x); //Check deze berekening van atan naar radians met de yaw ofset.
    yaw += glm::radians(yaw_offset);

    //Recalculate the direction vector
    direction.x = cos(glm::radians(pitch)) * cos(yaw);
    direction.y = sin(glm::radians(pitch));
    direction.z = cos(glm::radians(pitch)) * sin(yaw);
    direction = glm::normalize(direction);

    //Recalculate right vector to ensure perpendicularity then recalculate the up vector
    glm::vec3 right = glm::normalize(glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
    up = glm::normalize(glm::cross(right, direction));
}