import { getAuthToken } from "@/features/auth/authStore";
import { apiFetch } from "@/shared/api/client";
import type { Resource, ResourceFormValues } from "./types";

// Resource feature API: mirrors the C++ /api/resources endpoints.
type ResourceListResponse = {
  data: Resource[];
};

type ResourceStatusResponse = {
  status: "created" | "updated" | "deleted";
};

// Builds Authorization headers from the current browser auth session.
function authHeaders(): HeadersInit {
  const token = getAuthToken();

  return token ? { Authorization: `Bearer ${token}` } : {};
}

// Loads all resources owned by the authenticated user.
export async function listResources() {
  const response = await apiFetch<ResourceListResponse>("/api/resources", {
    headers: authHeaders(),
  });

  return response.data;
}

// Loads one resource by id for the authenticated user.
export function getResource(id: number) {
  return apiFetch<Resource>(`/api/resources?id=${id}`, {
    headers: authHeaders(),
  });
}

// Creates a text resource through the backend API.
export function createTextResource(values: ResourceFormValues) {
  return apiFetch<ResourceStatusResponse>("/api/resources", {
    method: "POST",
    headers: authHeaders(),
    body: JSON.stringify({
      title: values.title,
      content: values.content,
      is_file: false,
    }),
  });
}

// Creates file metadata after object storage upload succeeds.
export function createFileResource(values: ResourceFormValues) {
  return apiFetch<ResourceStatusResponse>("/api/resources", {
    method: "POST",
    headers: authHeaders(),
    body: JSON.stringify({
      title: values.title,
      content: values.content,
      is_file: true,
    }),
  });
}

// Updates the title/content fields for an existing resource.
export function updateTextResource(
  id: number,
  values: ResourceFormValues,
) {
  return apiFetch<ResourceStatusResponse>("/api/resources", {
    method: "PUT",
    headers: authHeaders(),
    body: JSON.stringify({
      id,
      title: values.title,
      content: values.content,
    }),
  });
}

// Deletes one resource owned by the authenticated user.
export function deleteResource(id: number) {
  return apiFetch<ResourceStatusResponse>(`/api/resources?id=${id}`, {
    method: "DELETE",
    headers: authHeaders(),
  });
}
