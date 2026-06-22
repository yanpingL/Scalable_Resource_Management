import { ResourceDetail } from "@/features/resources/components/ResourceDetail";

type ResourceDetailPageProps = {
  params: Promise<{
    id: string;
  }>;
};

// Converts the dynamic route id into the resource detail component.
export default async function ResourceDetailPage({
  params,
}: ResourceDetailPageProps) {
  // App Router page files translate URL segments into feature components.
  const { id } = await params;

  return <ResourceDetail id={Number(id)} />;
}
