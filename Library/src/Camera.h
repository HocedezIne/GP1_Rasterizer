#pragma once
#include <cassert>
#include <SDL_keyboard.h>
#include <SDL_mouse.h>

#include "Maths.h"
#include "Timer.h"

namespace dae
{
	struct Camera
	{
		Camera() = default;

		Camera(const Vector3& _origin, float _fovAngle):
			origin{_origin},
			fovAngle{_fovAngle}
		{
		}


		Vector3 origin{};
		float fovAngle{90.f};
		float fov{ tanf((fovAngle * TO_RADIANS) / 2.f) };
		float aspectRatio{};

		Vector3 forward{Vector3::UnitZ};
		Vector3 up{Vector3::UnitY};
		Vector3 right{Vector3::UnitX};

		float totalPitch{};
		float totalYaw{};

		const float movementSpeed{ 10.f };
		bool updateONB{ true };

		Matrix invViewMatrix{};
		Matrix viewMatrix{};
		Matrix projectionMatrix{};

		void Initialize(float _aspectRatio, float _fovAngle = 90.f, Vector3 _origin = {0.f,0.f,0.f})
		{
			fovAngle = _fovAngle;
			fov = tanf((fovAngle * TO_RADIANS) / 2.f);

			origin = _origin;

			aspectRatio = _aspectRatio;

			//CalculateProjectionMatrix();
		}

		void CalculateViewMatrix()
		{
			if (updateONB)
			{
				forward.Normalize();
				right = Vector3::Cross(Vector3::UnitY, forward);
				right.Normalize();
				up = Vector3::Cross(forward, right);
				up.Normalize();
			}
			updateONB = false;

			viewMatrix = Matrix::CreateLookAtLH(origin, forward, up);
		}

		void CalculateProjectionMatrix()
		{
			const float near{ 0.1f };
			const float far{ 100.f };

			projectionMatrix = Matrix::CreatePerspectiveFovLH(fov, aspectRatio, near, far);
		}

		void Update(Timer* pTimer)
		{
			const float deltaTime = pTimer->GetElapsed();

			//Keyboard Input
			const uint8_t* pKeyboardState = SDL_GetKeyboardState(nullptr);


			//Mouse Input
			int mouseX{}, mouseY{};
			const uint32_t mouseState = SDL_GetRelativeMouseState(&mouseX, &mouseY);

			// Keyboard
			if (pKeyboardState[SDL_SCANCODE_W])
			{
				origin += forward * movementSpeed * deltaTime;
				updateONB = true;
			}
			if (pKeyboardState[SDL_SCANCODE_S])
			{
				origin -= forward * movementSpeed * deltaTime;
				updateONB = true;
			}
			if (pKeyboardState[SDL_SCANCODE_D])
			{
				origin += right * movementSpeed * deltaTime;
				updateONB = true;
			}
			if (pKeyboardState[SDL_SCANCODE_A])
			{
				origin -= right * movementSpeed * deltaTime;
				updateONB = true;
			}

			// Mouse
			if (mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT) && mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) // move world up/down
			{
				origin -= up * float(mouseY) * movementSpeed * deltaTime;
				updateONB = true;
			}
			else if (mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT)) // rotate yaw and pitch
			{
				const float totalYaw{ (float(mouseX) * deltaTime) / 2 };
				const float totalPitch{ (float(mouseY) * deltaTime) / 2 };

				Matrix rotation = Matrix::CreateRotation(totalPitch, totalYaw, 0);
				forward = rotation.TransformVector(forward);

				updateONB = true;
			}
			else if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT))
			{
				origin -= forward * float(mouseY) * movementSpeed * deltaTime; // move foward/backward

				// rotate yaw
				const float totalYaw{ (float(mouseX) * deltaTime) / 2 };

				Matrix rotation = Matrix::CreateRotation(0, totalYaw, 0);
				forward = rotation.TransformVector(forward);

				updateONB = true;
			}

			//Update Matrices
			CalculateViewMatrix();
			CalculateProjectionMatrix();
		}
	};
}
