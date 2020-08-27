#include "events.hpp"

#include <libfilezilla/file.hpp>
#include <libfilezilla/format.hpp>
#include <libfilezilla/mutex.hpp>

#define UPLINK_DISABLE_NAMESPACE_COMPAT
typedef bool _Bool;
#include <uplink/uplink.h>

#include <memory>

namespace {

fz::mutex output_mutex;

void fzprintf(storjEvent event)
{
	fz::scoped_lock l(output_mutex);

	fputc('0' + static_cast<int>(event), stdout);

	fflush(stdout);
}

template<typename ...Args>
void fzprintf(storjEvent event, Args &&... args)
{
	fz::scoped_lock l(output_mutex);

	fputc('0' + static_cast<int>(event), stdout);

	std::string s = fz::sprintf(std::forward<Args>(args)...);
	fwrite(s.c_str(), s.size(), 1, stdout);

	fputc('\n', stdout);
	fflush(stdout);
}

void print_error(std::string_view fmt, UplinkError const* err)
{
	std::string_view msg(err->message, fz::strlen(err->message));
	auto lines = fz::strtok_view(msg, "\r\n");

	fzprintf(storjEvent::Error, fmt, lines.empty() ? std::string_view() : lines.front());

	for (size_t i = 1; i < lines.size(); ++i) {
		fz::trim(lines[i]);
		fzprintf(storjEvent::Verbose, "%s", lines[i]);
	}
}

bool getLine(std::string & line)
{
	line.clear();
	while (true) {
		int c = fgetc(stdin);
		if (c == -1) {
			return false;
		}
		else if (!c) {
			return line.empty();
		}
		else if (c == '\n') {
			return !line.empty();
		}
		else if (c == '\r') {
			continue;
		}
		else {
			line += static_cast<unsigned char>(c);
		}
	}
}

std::string next_argument(std::string & line)
{
	std::string ret;

	fz::trim(line);

	if (line[0] == '"') {
		size_t pos = 1;
		size_t pos2;
		while ((pos2 = line.find('"', pos)) != std::string::npos && line[pos2 + 1] == '"') {
			ret += line.substr(pos, pos2 - pos + 1);
			pos = pos2 + 2;
		}
		if (pos2 == std::string::npos || (line[pos2 + 1] != ' ' && line[pos2 + 1] != '\0')) {
			line.clear();
			ret.clear();
		}
		ret += line.substr(pos, pos2 - pos);
		line = line.substr(pos2 + 1);
	}
	else {
		size_t pos = line.find(' ');
		if (pos == std::string::npos) {
			ret = line;
			line.clear();
		}
		else {
			ret = line.substr(0, pos);
			line = line.substr(pos + 1);
		}
	}

	fz::trim(line);

	return ret;
}

void listBuckets(UplinkProject *project)
{
	UplinkBucketIterator *it = uplink_list_buckets(project, nullptr);

	while (uplink_bucket_iterator_next(it)) {
		UplinkBucket *bucket = uplink_bucket_iterator_item(it);
		std::string name = bucket->name;
		fz::replace_substrings(name, "\r", "");
		fz::replace_substrings(name, "\n", "");
		fzprintf(storjEvent::Listentry, "%s\n-1\n%d", name, bucket->created);
		uplink_free_bucket(bucket);
	}
	UplinkError *err = uplink_bucket_iterator_err(it);
	if (err) {
		print_error("bucket listing failed: %s", err);
		uplink_free_error(err);
		uplink_free_bucket_iterator(it);
		return;
	}

	uplink_free_bucket_iterator(it);

	fzprintf(storjEvent::Done);
}

void listObjects(UplinkProject *project, std::string const& bucket, std::string const& prefix)
{
	if (!prefix.empty() && prefix.back() != '/') {
		listObjects(project, bucket, prefix + '/');
	}

	UplinkListObjectsOptions options = {
		prefix.c_str(),
		"",
		false,
		true,
		true,
	};

	UplinkObjectIterator *it = uplink_list_objects(project, bucket.c_str(), &options);

	while (uplink_object_iterator_next(it)) {
		UplinkObject *object = uplink_object_iterator_item(it);

		std::string objectName = object->key;
		if (!prefix.empty()) {
			size_t pos = objectName.find(prefix);
			if (pos != 0) {
				continue;
			}
			objectName = objectName.substr(prefix.size());
		}
		if (objectName != ".") {
			fzprintf(storjEvent::Listentry, "%s\n%d\n%d", objectName, object->system.content_length, objectName, object->system.created);
		}

		uplink_free_object(object);
	}

	UplinkError *err = uplink_object_iterator_err(it);
	if (err) {
		print_error("object listing failed: %s", err);
		uplink_free_error(err);
		uplink_free_object_iterator(it);
		return;
	}
	uplink_free_object_iterator(it);

	fzprintf(storjEvent::Done);
}

bool close_and_free_download(UplinkDownloadResult& download_result, bool silent)
{
	UplinkError *close_error = uplink_close_download(download_result.download);
	if (close_error) {
		if (!silent) {
			print_error("download failed to close: %s", close_error);
		}
		uplink_free_error(close_error);
		uplink_free_download_result(download_result);
		return false;
	}

	uplink_free_download_result(download_result);

	return true;
}

void downloadObject(UplinkProject *project, std::string bucket, std::string const& id, std::string const& file)
{
	UplinkDownloadResult download_result = uplink_download_object(project, bucket.c_str(), id.c_str(), nullptr);
	if (download_result.error) {
		print_error("download starting failed: %s", download_result.error);
		uplink_free_download_result(download_result);
		return;
	}

	fz::file f(fz::to_native(fz::to_wstring_from_utf8(file)), fz::file::writing, fz::file::empty);
	if (!f.opened()) {
		fzprintf(storjEvent::Error, "Could not local file for writing");
		close_and_free_download(download_result, true);
		return;
	}

	size_t const buffer_size = 32768;
	auto buffer = std::make_unique<char[]>(buffer_size);

	UplinkDownload *download = download_result.download;

	size_t downloaded_total = 0;

	while (true) {
		UplinkReadResult result = uplink_download_read(download, buffer.get(), buffer_size);
		downloaded_total += result.bytes_read;


		if (result.error) {
			if (result.error->code == EOF) {
				uplink_free_read_result(result);
				break;
			}
			print_error("download failed receiving data: %s", result.error);
			uplink_free_read_result(result);
			close_and_free_download(download_result, true);

			return;
		}

		int written = f.write(buffer.get(), result.bytes_read);

		uplink_free_read_result(result);

		if (written < 0 || static_cast<size_t>(written) != result.bytes_read) {
			fzprintf(storjEvent::Error, "Could not write to local file");
			close_and_free_download(download_result, true);
			return;
		}

		fzprintf(storjEvent::Transfer, "%u", result.bytes_read);
	}

	if (!close_and_free_download(download_result, false)) {
		return;
	}

	fzprintf(storjEvent::Done);
}

void uploadObject(UplinkProject *project, std::string const& bucket, std::string const& prefix, std::string const& file, std::string const& objectName)
{
	std::string object_key = bucket;
	if (!prefix.empty()) {
		object_key += "/" + prefix;
	}

	fz::file f;
	if (!file.empty()) {
		if (!f.open(fz::to_native(fz::to_wstring_from_utf8(file)), fz::file::reading, fz::file::existing)) {
			fzprintf(storjEvent::Error, "Could not open local file");
			return;
		}
	}

	// get length of file:
	size_t length = file.empty() ? 0 : f.size();

	size_t const buffer_size = 32768;
	auto buffer = std::make_unique<char[]>(buffer_size);

	UplinkUploadResult upload_result = uplink_upload_object(project, object_key.c_str(), objectName.c_str(), nullptr);

	if (upload_result.error) {
		print_error("upload failed: %s", upload_result.error);
		uplink_free_upload_result(upload_result);
		return;
	}

	if (!upload_result.upload->_handle) {
		fzprintf(storjEvent::Error, "Missing upload handle");
		uplink_free_upload_result(upload_result);
		return;
	}

	UplinkUpload *upload = upload_result.upload;

	size_t uploaded_total = 0;

	while (uploaded_total < length) {

		int r = f.read(buffer.get(), buffer_size);
		if (r <= 0) {
			fzprintf(storjEvent::Error, "Could not read from local file");
			uplink_free_upload_result(upload_result);
			return;
		}
		UplinkWriteResult result = uplink_upload_write(upload, buffer.get(), r);

		if (result.error) {
			print_error("upload failed: %s", result.error);
			uplink_free_write_result(result);
			uplink_free_upload_result(upload_result);
			return;
		}

		if (!result.bytes_written) {
			fzprintf(storjEvent::Error, "upload_write did not write anything");
			uplink_free_write_result(result);
			uplink_free_upload_result(upload_result);
			return;
		}
		uploaded_total += result.bytes_written;

		fzprintf(storjEvent::Transfer, "%u", result.bytes_written);
		uplink_free_write_result(result);
	}

	UplinkError *commit_err = uplink_upload_commit(upload);
	if (commit_err) {
		print_error("finalizing upload failed: %s", commit_err);
		uplink_free_error(commit_err);
		uplink_free_upload_result(upload_result);
		return;
	}

	uplink_free_upload_result(upload_result);

	fzprintf(storjEvent::Done);
}

void deleteObject(UplinkProject *project, std::string bucketName, std::string objectKey)
{
	UplinkObjectResult object_result = uplink_delete_object(project, bucketName.c_str(), objectKey.c_str());
	if (object_result.error) {
		print_error("failed to create bucket: %s", object_result.error);
		uplink_free_object_result(object_result);
		return;
	}

	fzprintf(storjEvent::Status, "deleted object %s", objectKey);
	uplink_free_object_result(object_result);
	fzprintf(storjEvent::Done);
}

void createBucket(UplinkProject *project, std::string bucketName)
{
	UplinkBucketResult bucket_result = uplink_ensure_bucket(project, bucketName.c_str());
	if (bucket_result.error) {
		print_error("failed to create bucket: %s", bucket_result.error);
		uplink_free_bucket_result(bucket_result);
		return;
	}

	UplinkBucket *bucket = bucket_result.bucket;
	fzprintf(storjEvent::Status, "created bucket %s", bucket->name);
	uplink_free_bucket_result(bucket_result);

	fzprintf(storjEvent::Done);
}

void deleteBucket(UplinkProject *project, std::string bucketName)
{
	UplinkBucketResult bucket_result = uplink_delete_bucket(project, bucketName.c_str());
	if (bucket_result.error) {
		print_error("Failed to remove bucket: %s", bucket_result.error);
		uplink_free_bucket_result(bucket_result);
		return;
	}

	if (bucket_result.bucket) {
		UplinkBucket *bucket = bucket_result.bucket;
		fzprintf(storjEvent::Status, "deleted bucket %s", bucket->name);
	}
	uplink_free_bucket_result(bucket_result);

	fzprintf(storjEvent::Done);
}

std::pair<std::string, std::string> SplitPath(std::string_view path)
{
	std::pair<std::string, std::string> ret;

	if (!path.empty()) {
		size_t pos = path.find('/', 1);
		if (pos != std::string::npos) {
			ret.first = path.substr(1, pos - 1);
			ret.second = path.substr(pos + 1);
		}
		else {
			ret.first = path.substr(1);
		}
	}
	return ret;
}
}

int main()
{
	fzprintf(storjEvent::Reply, "fzStorj started, protocol_version=%d", FZSTORJ_PROTOCOL_VERSION);

	std::string satelliteURL;
	std::string apiKey;
	std::string encryptionPassPhrase;
	std::string serializedAccessGrantKey;

	UplinkProjectResult project_result{};

	UplinkConfig config = {
		"FileZilla",
		0,
		nullptr
	};

	auto openStorjProject = [&]() -> bool {
		if (project_result.project) {
			return true;
		}
		UplinkAccessResult access_result;
		if (!apiKey.empty()) {
			access_result = uplink_config_request_access_with_passphrase(config, satelliteURL.c_str(), apiKey.c_str(), encryptionPassPhrase.c_str());
			if (access_result.error) {
				print_error("failed to parse access: %s", access_result.error);
				uplink_free_access_result(access_result);
				return false;
			}
		}
		else {
			access_result = uplink_parse_access(serializedAccessGrantKey.c_str());
			if (access_result.error) {
				print_error("failed to parse access: %s", access_result.error);
				uplink_free_access_result(access_result);
				return false;
			}
		}

		project_result = uplink_config_open_project(config, access_result.access);
		uplink_free_access_result(access_result);
		if (project_result.error) {
			print_error("failed to open project: %s", project_result.error);
			uplink_free_project_result(project_result);
			project_result = UplinkProjectResult{};
			return false;
		}

		return true;
	};

	int ret = 0;
	while (true) {
		std::string command;
		if (!getLine(command)) {
			ret = 1;
			break;
		}

		if (command.empty()) {
			break;
		}

		std::size_t pos = command.find(' ');
		std::string arg;
		if (pos != std::string::npos) {
			arg = command.substr(pos + 1);
			command = command.substr(0, pos);
		}

		if (command == "host") {
			serializedAccessGrantKey = satelliteURL = next_argument(arg);
			fzprintf(storjEvent::Done);
		}
		else if (command == "key") {
			apiKey = next_argument(arg);
			fzprintf(storjEvent::Done);
		}
		else if (command == "pass") {
			encryptionPassPhrase = next_argument(arg);
			fzprintf(storjEvent::Done);
		}
		else if (command == "list") {

			auto [bucket, prefix] = SplitPath(next_argument(arg));

			if (bucket.empty()) {
				if (!openStorjProject()) {
					continue;
				}
				listBuckets(project_result.project);
			}
			else {
				if (!prefix.empty() && prefix.back() != '/') {
					prefix += '/';
				}

				if (!openStorjProject()) {
					continue;
				}
				listObjects(project_result.project, bucket, prefix);
			}
		}
		else if (command == "get") {
			auto [bucket, key] = SplitPath(next_argument(arg));
			std::string file = next_argument(arg);

			if (file.empty() || bucket.empty() || key.empty() || key.back() == '/') {
				fzprintf(storjEvent::Error, "Bad arguments");
				continue;
			}

			if (!openStorjProject()) {
				continue;
			}

			downloadObject(project_result.project, bucket, key, file);
		}
		else if (command == "put") {
			std::string file = next_argument(arg);
			auto [bucket, key] = SplitPath(next_argument(arg));

			if (file.empty() || bucket.empty() || key.empty() || key.back() == '/') {
				fzprintf(storjEvent::Error, "Bad arguments");
				continue;
			}

			if (file == "null" ) {
				file.clear();
			}

			std::string prefix;
			std::string objectName;

			size_t pos = key.rfind('/');
			if (pos != std::string::npos) {
				prefix = key.substr(0, pos);
				objectName = key.substr(pos + 1);
			}
			else {
				objectName = key;
			}

			if (!openStorjProject()) {
				continue;
			}
			uploadObject(project_result.project, bucket, prefix, file, objectName);
		}
		else if (command == "rm") {
			auto [bucket, key] = SplitPath(next_argument(arg));
			if (bucket.empty() || key.empty()) {
				fzprintf(storjEvent::Error, "Bad arguments");
				continue;
			}

			if (!openStorjProject()) {
				continue;
			}

			deleteObject(project_result.project, bucket, key);
		}
		else if (command == "mkbucket") {
			std::string bucketName = next_argument(arg);
			if (bucketName.empty()) {
				fzprintf(storjEvent::Error, "Bad arguments");
				continue;
			}

			if (!openStorjProject()) {
				continue;
			}
			createBucket(project_result.project, bucketName);
		}
		else if (command == "rmbucket") {
			std::string bucketName = next_argument(arg);
			if (bucketName.empty()) {
				fzprintf(storjEvent::Error, "Bad arguments");
				continue;
			}

			if (!openStorjProject()) {
				continue;
			}
			deleteBucket(project_result.project, bucketName);
		}
		else {
			fzprintf(storjEvent::Error, "No such command: %s", command);
		}
	}

	return ret;
}
