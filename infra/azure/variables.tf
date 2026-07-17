variable "subscription_id" {
  type        = string
  description = "Azure subscription ID."
}

variable "project_name" {
  type        = string
  description = "Short name prefix for Azure resources."
  default     = "webserver"
}

variable "resource_group_name" {
  type        = string
  description = "Azure resource group name."
  default     = "rg-webserver"
}

variable "location" {
  type        = string
  description = "Azure region."
  default     = "australiaeast"
}

variable "vm_size" {
  type        = string
  description = "Azure VM size. B2pts_v2 is ARM-based."
  default     = "Standard_B2pts_v2"
}

variable "admin_username" {
  type        = string
  description = "Linux admin user."
  default     = "azureuser"
}

variable "ssh_public_key_path" {
  type        = string
  description = "Path to the SSH public key for VM login."
  default     = "~/.ssh/webserver_azure.pub"
}

variable "ssh_source_address_prefix" {
  type        = string
  description = "CIDR allowed to SSH. Use your public IP with /32 when possible."
  default     = "*"
}

variable "os_disk_size_gb" {
  type        = number
  description = "OS disk size in GiB."
  default     = 64
}

variable "os_disk_storage_account_type" {
  type        = string
  description = "OS disk type."
  default     = "Standard_LRS"
}

variable "deploy_path" {
  type        = string
  description = "Directory used by deployment scripts on the VM."
  default     = "/opt/webserver"
}
