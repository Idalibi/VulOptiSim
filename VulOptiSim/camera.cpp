#include "pch.h"
#include "camera.h"

// Initialiseert de camera met een positie, kijkrichting en bewegingssnelheden.
Camera::Camera(glm::vec3 position, glm::vec3 up, glm::vec3 direction, float movement_speed, float rotation_speed)
    : position(position),
    up(glm::normalize(up)),
    direction(glm::normalize(direction)),
    movement_speed(movement_speed),
    rotation_speed(rotation_speed)
{
}

// Geeft de viewmatrix terug die bepaalt hoe de wereld vanuit de camera wordt weergegeven.
glm::mat4 Camera::get_view_matrix() const
{
    return glm::lookAt(position, position + direction, up);
}

// Geeft de huidige positie van de camera terug.
glm::vec3 Camera::get_position() const
{
    return position;
}

// Stelt een nieuwe positie in voor de camera.
void Camera::set_position(const glm::vec3& new_position)
{
    position = new_position;
}

// Verplaatst de camera relatief ten opzichte van de huidige positie.
void Camera::update_position(const glm::vec3& position_offset)
{
    position += position_offset;
}

// Geeft de huidige up-vector terug, die de verticale oriëntatie van de camera bepaalt.
glm::vec3 Camera::get_up() const
{
    return up;
}

// Stelt een nieuwe up-vector in en normaliseert deze.
void Camera::set_up(const glm::vec3& new_up)
{
    up = glm::normalize(new_up);
}

// Past de up-vector aan met behulp van een transformatiematrix.
void Camera::update_up(const glm::mat4& transformation_matrix)
{
    up = transformation_matrix * glm::vec4(up, 1.0f);
    up = glm::normalize(up);
}

// Verplaatst de camera vooruit over het horizontale XZ-vlak.
void Camera::move_forward(float distance)
{
    // Negeer de verticale component van de kijkrichting.
    glm::vec3 horizontal_direction = glm::normalize(glm::vec3(direction.x, 0.0f, direction.z));

    // Pas de verplaatsing aan op basis van de ingestelde bewegingssnelheid.
    update_position(horizontal_direction * distance * movement_speed);
}

// Verplaatst de camera achteruit door de voorwaartse beweging om te keren.
void Camera::move_backward(float distance)
{
    move_forward(-distance);
}

// Verplaatst de camera naar rechts over het horizontale vlak.
void Camera::move_right(float distance)
{
    // Bereken de zijwaartse richting met het kruisproduct van de kijkrichting en de up-vector.
    glm::vec3 side_axis = glm::normalize(glm::cross(glm::vec3(direction.x, 0.0f, direction.z), up));

    // Verplaats de camera in de berekende richting.
    update_position(side_axis * distance * movement_speed);
}

// Verplaatst de camera naar links door de rechtse beweging om te keren.
void Camera::move_left(float distance)
{
    move_right(-distance);
}

// Verplaatst de camera verticaal omhoog.
void Camera::move_up(float distance)
{
    update_position(glm::vec3(0.0f, movement_speed * distance, 0.0f));
}

// Verplaatst de camera omlaag door de opwaartse beweging om te keren.
void Camera::move_down(float distance)
{
    move_up(-distance);
}

// Draait de kijkrichting van de camera naar links rond de verticale Y-as.
void Camera::rotate_left(float speed)
{
    // Maak een rotatiematrix op basis van de rotatiesnelheid en de opgegeven snelheid.
    glm::mat4 rotation = glm::rotate(
        glm::mat4(1.0f),
        rotation_speed * speed * glm::radians(1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    // Pas de rotatie toe op de kijkrichting.
    direction = rotation * glm::vec4(direction, 1.0f);

    // Bereken de rechterrichting en werk de up-vector bij om de oriëntatie te behouden.
    glm::vec3 right = glm::normalize(glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
    up = glm::normalize(glm::cross(right, direction));
}

// Draait de camera naar rechts door de linkse rotatie om te keren.
void Camera::rotate_right(float speed)
{
    rotate_left(-speed);
}

// Geeft de huidige kijkrichting van de camera terug.
glm::vec3 Camera::get_direction() const
{
    return direction;
}

// Stelt een nieuwe kijkrichting in en werkt de up-vector bij.
void Camera::set_direction(const glm::vec3& new_direction)
{
    direction = glm::normalize(new_direction);

    // Bereken de loodrechte richtingsvectoren opnieuw.
    glm::vec3 right = glm::normalize(glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
    up = glm::normalize(glm::cross(right, direction));
}

// Past de kijkrichting aan met een transformatiematrix en normaliseert het resultaat.
void Camera::update_direction(const glm::mat4& transformation_matrix)
{
    direction = transformation_matrix * glm::vec4(direction, 1.0f);
    direction = glm::normalize(direction);
}

// Werkt de kijkrichting bij op basis van horizontale en verticale invoer.
// De X-component bepaalt de yaw (horizontale draaiing) en de Y-component de pitch (verticale draaiing).
void Camera::update_direction(const glm::vec2& offset)
{
    float yaw_offset = offset.x * rotation_speed;
    float pitch_offset = -offset.y * rotation_speed;

    // Bepaal de huidige pitch en pas de verticale invoer toe.
    float pitch = glm::degrees(glm::asin(direction.y));
    pitch += pitch_offset;

    // Beperk de pitch om te voorkomen dat de camera over de kop draait.
    if (pitch > 89.0f)
    {
        pitch = 89.0f;
    }

    if (pitch < -89.0f)
    {
        pitch = -89.0f;
    }

    // Bepaal de huidige yaw en pas de horizontale invoer toe.
    float yaw = glm::atan(direction.z, direction.x);
    yaw += glm::radians(yaw_offset);

    // Zet de hoeken om naar een nieuwe driedimensionale kijkrichting.
    direction.x = cos(glm::radians(pitch)) * cos(yaw);
    direction.y = sin(glm::radians(pitch));
    direction.z = cos(glm::radians(pitch)) * sin(yaw);
    direction = glm::normalize(direction);

    // Werk de zijwaartse richting en de up-vector bij op basis van de nieuwe kijkrichting.
    glm::vec3 right = glm::normalize(glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
    up = glm::normalize(glm::cross(right, direction));
}