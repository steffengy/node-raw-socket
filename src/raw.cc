#ifndef RAW_CC
#define RAW_CC

#include <nan.h>
#include <v8.h>
#include <node_buffer.h>

#include <stdio.h>
#include <string.h>
#include "raw.h"

using namespace v8;

#ifdef _WIN32
static char errbuf[1024];
#endif
const char* raw_strerror (int code) {
#ifdef _WIN32
	if (FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, 0, code, 0, errbuf,
			1024, NULL)) {
		return errbuf;
	} else {
		strcpy (errbuf, "Unknown error");
		return errbuf;
	}
#else
	return strerror (code);
#endif
}

static uint16_t checksum (uint16_t start_with, unsigned char *buffer,
		size_t length) {
	unsigned i;
	uint32_t sum = start_with > 0 ? ~start_with & 0xffff : 0;

	for (i = 0; i < (length & ~1U); i += 2) {
		sum += (uint16_t) ntohs (*((uint16_t *) (buffer + i)));
		if (sum > 0xffff)
			sum -= 0xffff;
	}
	if (i < length) {
		sum += buffer [i] << 8;
		if (sum > 0xffff)
			sum -= 0xffff;
	}

	return ~sum & 0xffff;
}

namespace raw {

static Persistent<String> CloseSymbol;
static Persistent<String> EmitSymbol;
static Persistent<String> ErrorSymbol;
static Persistent<String> RecvReadySymbol;
static Persistent<String> SendReadySymbol;

void InitAll (Handle<Object> target) {
	NanAssignPersistent<String>(CloseSymbol, NanNew<String>("close"));
	NanAssignPersistent<String>(EmitSymbol, NanNew<String>("emit"));
	NanAssignPersistent<String>(ErrorSymbol, NanNew<String>("error"));
	NanAssignPersistent<String>(RecvReadySymbol, NanNew<String>("recvReady"));
	NanAssignPersistent<String>(SendReadySymbol, NanNew<String>("sendReady"));

	ExportConstants (target);
	ExportFunctions (target);

	SocketWrap::Init (target);
}

NODE_MODULE(raw, InitAll)

NAN_METHOD(CreateChecksum) {
	NanScope();

	if (args.Length () < 2) {
		NanThrowError ("At least one argument is required");
		NanReturnValue (args.This ());
	}

	if (! args[0]->IsUint32 ()) {
		NanThrowError ("Start with argument must be an unsigned integer");
		NanReturnValue (args.This ());
	}

	uint32_t start_with = args[0]->ToUint32 ()->Value ();

	if (start_with > 65535) {
		NanThrowError ("Start with argument cannot be larger than 65535");
		NanReturnValue (args.This ());
	}

	if (! node::Buffer::HasInstance (args[1])) {
		NanThrowError ("Buffer argument must be a node Buffer object");
		NanReturnValue (args.This ());
	}

	Local<Object> buffer = args[1]->ToObject ();
	char *data = node::Buffer::Data (buffer);
	size_t length = node::Buffer::Length (buffer);
	unsigned int offset = 0;

	if (args.Length () > 2) {
		if (! args[2]->IsUint32 ()) {
			NanThrowError ("Offset argument must be an unsigned integer");
			NanReturnValue (args.This ());
		}
		offset = args[2]->ToUint32 ()->Value ();
		if (offset >= length) {
			NanThrowError ("Offset argument must be smaller than length of the buffer");
			NanReturnValue (args.This ());
		}
	}

	if (args.Length () > 3) {
		if (! args[3]->IsUint32 ()) {
			NanThrowError ("Length argument must be an unsigned integer");
	    	NanReturnValue (args.This ());
		}
		unsigned int new_length = args[3]->ToUint32 ()->Value ();
		if (new_length > length) {
			NanThrowError ("Length argument must be smaller than length of the buffer");
			NanReturnValue (args.This ());
		}
		length = new_length;
	}

	uint16_t sum = checksum ((uint16_t) start_with,
			(unsigned char *) data + offset, length);

	NanReturnValue (NanNew<Integer>(sum));
}

NAN_METHOD(Htonl) {
	NanScope();

	if (args.Length () < 1) {
		NanThrowError ("One arguments is required");
		NanReturnValue (args.This ());
	}

	if (! args[0]->IsUint32 ()) {
		NanThrowError ("Number must be a 32 unsigned integer");
		NanReturnValue (args.This ());
	}

	unsigned int number = args[0]->ToUint32 ()->Value ();
	Local<Integer> converted = NanNew<Integer>(htonl (number));

	NanReturnValue(converted);
}

NAN_METHOD(Htons) {
	NanScope();

	if (args.Length () < 1) {
		NanThrowError ("One arguments is required");
		NanReturnValue (args.This ());
	}

	if (! args[0]->IsUint32 ()) {
		NanThrowError ("Number must be a 16 unsigned integer");
		NanReturnValue (args.This ());
	}

	unsigned int number = args[0]->ToUint32 ()->Value ();
	if (number > 65535) {
		NanThrowError ("Number cannot be larger than 65535");
		NanReturnValue (args.This ());
	}

	NanReturnValue(NanNew<Integer>(htons (number)));
}

NAN_METHOD(Ntohl) {
	NanScope();

	if (args.Length () < 1) {
		NanThrowError ("One arguments is required");
		NanReturnValue (args.This ());
	}

	if (! args[0]->IsUint32 ()) {
		NanThrowError ("Number must be a 32 unsigned integer");
		NanReturnValue (args.This ());
	}

	unsigned int number = args[0]->ToUint32 ()->Value ();

	NanReturnValue(NanNew<Integer>(ntohl (number)));
}

NAN_METHOD(Ntohs) {
	NanScope();

	if (args.Length () < 1) {
		NanThrowError ("One arguments is required");
		NanReturnValue (args.This ());
	}

	if (! args[0]->IsUint32 ()) {
		NanThrowError ("Number must be a 16 unsigned integer");
		NanReturnValue (args.This ());
	}

	unsigned int number = args[0]->ToUint32 ()->Value ();
	if (number > 65535) {
		NanThrowError ("Number cannot be larger than 65535");
		NanReturnValue (args.This ());
	}

	NanReturnValue(NanNew<Integer>(htons (number)));
}

void ExportConstants (Handle<Object> target) {
	Local<Object> socket_level = NanNew<Object>();
	Local<Object> socket_option = NanNew<Object>();

	target->Set (NanNew<String> ("SocketLevel"), socket_level);
	target->Set (NanNew<String>("SocketOption"), socket_option);

	socket_level->Set (NanNew<String>("SOL_SOCKET"), NanNew<Number> (SOL_SOCKET));
	socket_level->Set (NanNew<String>("IPPROTO_IP"), NanNew<Number> ((int)IPPROTO_IP));
	socket_level->Set (NanNew<String>("IPPROTO_IPV6"), NanNew<Number> ((int)IPPROTO_IPV6));

	socket_option->Set (NanNew<String>("SO_BROADCAST"), NanNew<Number> (SO_BROADCAST));
	socket_option->Set (NanNew<String>("SO_RCVBUF"), NanNew<Number> (SO_RCVBUF));
	socket_option->Set (NanNew<String>("SO_RCVTIMEO"), NanNew<Number> (SO_RCVTIMEO));
	socket_option->Set (NanNew<String>("SO_SNDBUF"), NanNew<Number> (SO_SNDBUF));

	#ifdef SO_BINDTODEVICE
	socket_option->Set (NanNew<String>("SO_BINDTODEVICE"), NanNew<Number> (SO_BINDTODEVICE));
	#endif

	socket_option->Set (NanNew<String>("SO_SNDTIMEO"), NanNew<Number> (SO_SNDTIMEO));

	socket_option->Set (NanNew<String>("IP_HDRINCL"), NanNew<Number> (IP_HDRINCL));
	socket_option->Set (NanNew<String>("IP_OPTIONS"), NanNew<Number> (IP_OPTIONS));
	socket_option->Set (NanNew<String>("IP_TOS"), NanNew<Number> (IP_TOS));
	socket_option->Set (NanNew<String>("IP_TTL"), NanNew<Number> (IP_TTL));

#ifdef _WIN32
	socket_option->Set (NanNew<String>("IPV6_HDRINCL"), NanNew<Number> (IPV6_HDRINCL));
#endif
	socket_option->Set (NanNew<String>("IPV6_TTL"), NanNew<Number> (IPV6_UNICAST_HOPS));
	socket_option->Set (NanNew<String>("IPV6_UNICAST_HOPS"), NanNew<Number> (IPV6_UNICAST_HOPS));
	socket_option->Set (NanNew<String>("IPV6_V6ONLY"), NanNew<Number> (IPV6_V6ONLY));
}

void ExportFunctions (Handle<Object> target) {
	target->Set (NanNew<String>("createChecksum"), NanNew<FunctionTemplate> (CreateChecksum)->GetFunction ());

	target->Set (NanNew<String>("htonl"), NanNew<FunctionTemplate> (Htonl)->GetFunction ());
	target->Set (NanNew<String>("htons"), NanNew<FunctionTemplate> (Htons)->GetFunction ());
	target->Set (NanNew<String>("ntohl"), NanNew<FunctionTemplate> (Ntohl)->GetFunction ());
	target->Set (NanNew<String>("ntohs"), NanNew<FunctionTemplate> (Ntohs)->GetFunction ());
}

void SocketWrap::Init (Handle<Object> target) {
	NanScope();

	Local<FunctionTemplate> tpl = NanNew<FunctionTemplate> (New);

	tpl->InstanceTemplate ()->SetInternalFieldCount (1);
	tpl->SetClassName (NanNew<String>("SocketWrap"));

	NODE_SET_PROTOTYPE_METHOD(tpl, "close", Close);
	NODE_SET_PROTOTYPE_METHOD(tpl, "getOption", GetOption);
	NODE_SET_PROTOTYPE_METHOD(tpl, "pause", Pause);
	NODE_SET_PROTOTYPE_METHOD(tpl, "recv", Recv);
	NODE_SET_PROTOTYPE_METHOD(tpl, "send", Send);
	NODE_SET_PROTOTYPE_METHOD(tpl, "bind", Bind);
	NODE_SET_PROTOTYPE_METHOD(tpl, "setOption", SetOption);

	target->Set (NanNew<String> ("SocketWrap"), tpl->GetFunction ());
}

SocketWrap::SocketWrap () {
	deconstructing_ = false;
}

SocketWrap::~SocketWrap () {
	deconstructing_ = true;
	this->CloseSocket ();
}

NAN_METHOD(SocketWrap::Close) {
	NanScope();
	SocketWrap* socket = SocketWrap::Unwrap<SocketWrap> (args.This ());

	socket->CloseSocket ();

	NanReturnValue(args.This ());
}

void SocketWrap::CloseSocket (void) {
	NanScope();

	if (this->poll_initialised_) {
		uv_close ((uv_handle_t *) this->poll_watcher_, OnClose);
		closesocket (this->poll_fd_);
		this->poll_fd_ = INVALID_SOCKET;
		this->poll_initialised_ = false;
	}
  if (deconstructing_) {
    return;
  }

	Local<Value> emit = NanObjectWrapHandle(this)->Get (NanNew(EmitSymbol));
	Local<Function> cb = emit.As<Function> ();

	Local<Value> args[1];
	args[0] = NanNew(CloseSymbol);

	cb->Call (NanObjectWrapHandle(this), 1, args);
}

int SocketWrap::CreateSocket (void) {
	if (this->poll_initialised_)
		return 0;

	if ((this->poll_fd_ = socket (this->family_, SOCK_RAW, this->protocol_))
			== INVALID_SOCKET)
		return SOCKET_ERRNO;

#ifdef _WIN32
	unsigned long flag = 1;
	if (ioctlsocket (this->poll_fd_, FIONBIO, &flag) == SOCKET_ERROR)
		return SOCKET_ERRNO;
#else
	int flag = 1;
	if ((flag = fcntl (this->poll_fd_, F_GETFL, 0)) == SOCKET_ERROR)
		return SOCKET_ERRNO;
	if (fcntl (this->poll_fd_, F_SETFL, flag | O_NONBLOCK) == SOCKET_ERROR)
		return SOCKET_ERRNO;
#endif

	poll_watcher_ = new uv_poll_t;
	uv_poll_init_socket (uv_default_loop (), this->poll_watcher_,
			this->poll_fd_);
	this->poll_watcher_->data = this;
	uv_poll_start (this->poll_watcher_, UV_READABLE, IoEvent);

	this->poll_initialised_ = true;

	return 0;
}

NAN_METHOD(SocketWrap::GetOption) {
	NanScope();
	SocketWrap* socket = SocketWrap::Unwrap<SocketWrap> (args.This ());

	if (args.Length () < 3) {
		NanThrowError ("Three arguments are required");
		NanReturnValue (args.This ());
	}

	if (! args[0]->IsNumber ()) {
		NanThrowError ("Level argument must be a number");
		NanReturnValue (args.This ());
	}

	if (! args[1]->IsNumber ()) {
		NanThrowError ("Option argument must be a number");
		NanReturnValue (args.This ());
	}

	int level = args[0]->ToInt32 ()->Value ();
	int option = args[1]->ToInt32 ()->Value ();
	SOCKET_OPT_TYPE val = NULL;
	unsigned int ival = 0;
	SOCKET_LEN_TYPE len;

	if (! node::Buffer::HasInstance (args[2])) {
		NanThrowError ("Value argument must be a node Buffer object if length is provided");
		NanReturnValue (args.This ());
	}

	Local<Object> buffer = args[2]->ToObject ();
	val = node::Buffer::Data (buffer);

	if (! args[3]->IsInt32 ()) {
		NanThrowError ("Length argument must be an unsigned integer");
		NanReturnValue (args.This ());
	}

	len = (SOCKET_LEN_TYPE) node::Buffer::Length (buffer);

	int rc = getsockopt (socket->poll_fd_, level, option,
			(val ? val : (SOCKET_OPT_TYPE) &ival), &len);

	if (rc == SOCKET_ERROR) {
		NanThrowError (raw_strerror (SOCKET_ERRNO));
		NanReturnValue (args.This ());
	}

	Local<Number> got = NanNew<v8::Uint32>(len);

	NanReturnValue(got);
}

void SocketWrap::HandleIOEvent (int status, int revents) {
	NanScope();
	if (status) {
		Local<Value> emit = NanObjectWrapHandle(this)->Get (NanNew<String>(EmitSymbol));
		Local<Function> cb = emit.As<Function> ();

		Local<Value> args[2];
		args[0] = NanNew<String> (ErrorSymbol);
		args[1] = NanError(raw_strerror (status));

		cb->Call (NanObjectWrapHandle(this), 2, args);
	} else {
		Local<Value> emit = NanObjectWrapHandle(this)->Get (NanNew(EmitSymbol));
		Local<Function> cb = emit.As<Function> ();

		Local<Value> args[1];
		if (revents & UV_READABLE)
			args[0] = NanNew(RecvReadySymbol);
		else
			args[0] = NanNew(SendReadySymbol);

		cb->Call (NanObjectWrapHandle(this), 1, args);
	}
}

NAN_METHOD(SocketWrap::New) {
	NanScope();
	SocketWrap* socket = new SocketWrap ();
	int rc, family = AF_INET;

	if (args.Length () < 1) {
		NanThrowError ("One argument is required");
		NanReturnValue (args.This ());
	}

	if (! args[0]->IsUint32 ()) {
		NanThrowError ("Protocol argument must be an unsigned integer");
		NanReturnValue (args.This ());
	} else {
		socket->protocol_ = args[0]->ToUint32 ()->Value ();
	}

	if (args.Length () > 1) {
		if (! args[1]->IsUint32 ()) {
			NanThrowError ("Address family argument must be an unsigned integer");
	    	NanReturnValue (args.This ());
		} else {
			if (args[1]->ToUint32 ()->Value () == 2)
				family = AF_INET6;
		}
	}

	socket->family_ = family;

	socket->poll_initialised_ = false;

	socket->no_ip_header_ = false;

	rc = socket->CreateSocket ();
	if (rc != 0) {
		NanThrowError (raw_strerror (rc));
		NanReturnValue (args.This ());
	}

	socket->Wrap (args.This ());

	NanReturnValue (args.This ());
}

void SocketWrap::OnClose (uv_handle_t *handle) {
	delete handle;
}

NAN_METHOD(SocketWrap::Pause) {
	NanScope();
	SocketWrap* socket = SocketWrap::Unwrap<SocketWrap> (args.This ());

	if (args.Length () < 2) {
		NanThrowError ("Two arguments are required");
		NanReturnValue (args.This ());
	}

	if (! args[0]->IsBoolean ()) {
		NanThrowError ("Recv argument must be a boolean");
		NanReturnValue (args.This ());
	}
	bool pause_recv = args[0]->ToBoolean ()->Value ();

	if (! args[1]->IsBoolean ()) {
		NanThrowError ("Send argument must be a boolean");
		NanReturnValue (args.This ());
	}
	bool pause_send = args[1]->ToBoolean ()->Value ();

	int events = (pause_recv ? 0 : UV_READABLE)
			| (pause_send ? 0 : UV_WRITABLE);

	if (! socket->deconstructing_) {
		uv_poll_stop (socket->poll_watcher_);
		if (events)
			uv_poll_start (socket->poll_watcher_, events, IoEvent);
	}

	NanReturnValue (args.This ());
}

NAN_METHOD(SocketWrap::Recv) {
	NanScope();
	SocketWrap* socket = SocketWrap::Unwrap<SocketWrap> (args.This ());
	Local<Object> buffer;
	sockaddr_in sin_address;
	sockaddr_in6 sin6_address;
	char addr[50];
	int rc;

#ifdef _WIN32
	int sin_length = socket->family_ == AF_INET6
			? sizeof (sin6_address)
			: sizeof (sin_address);
#else
	socklen_t sin_length = socket->family_ == AF_INET6
			? sizeof (sin6_address)
			: sizeof (sin_address);
#endif

	if (args.Length () < 2) {
		NanThrowError ("Five arguments are required");
		NanReturnValue (args.This ());
	}

	if (! node::Buffer::HasInstance (args[0])) {
		NanThrowError ("Buffer argument must be a node Buffer object");
		NanReturnValue (args.This ());
	} else {
		buffer = args[0]->ToObject ();
	}

	if (! args[1]->IsFunction ()) {
		NanThrowError ("Callback argument must be a function");
		NanReturnValue (args.This ());
	}

	rc = socket->CreateSocket ();
	if (rc != 0) {
		NanThrowError (raw_strerror (errno));
		NanReturnValue (args.This ());
	}

	if (socket->family_ == AF_INET6) {
		memset (&sin6_address, 0, sizeof (sin6_address));
		rc = recvfrom (socket->poll_fd_, node::Buffer::Data (buffer),
				(int) node::Buffer::Length (buffer), 0, (sockaddr *) &sin6_address,
				&sin_length);
	} else {
		memset (&sin_address, 0, sizeof (sin_address));
		rc = recvfrom (socket->poll_fd_, node::Buffer::Data (buffer),
				(int) node::Buffer::Length (buffer), 0, (sockaddr *) &sin_address,
				&sin_length);
	}

	if (rc == SOCKET_ERROR) {
		NanThrowError (raw_strerror (SOCKET_ERRNO));
		NanReturnValue (args.This ());
	}

	if (socket->family_ == AF_INET6)
		uv_ip6_name (&sin6_address, addr, 50);
	else
		uv_ip4_name (&sin_address, addr, 50);

	Local<Function> cb = Local<Function>::Cast (args[1]);
	const unsigned argc = 3;
	Local<Value> argv[argc];
	argv[0] = args[0];
	argv[1] = NanNew<Number> (rc);
	argv[2] = NanNew<String> (addr);
	cb->Call (NanGetCurrentContext()->Global (), argc, argv);

	NanReturnValue (args.This ());
}

NAN_METHOD(SocketWrap::Bind) {
	NanScope();
	SocketWrap* socket = SocketWrap::Unwrap<SocketWrap> (args.This ());

	if (args.Length () < 2) {
		NanThrowError ("Two arguments are required");
		NanReturnValue (args.This ());
	}

	if (! args[0]->IsString ()) {
		NanThrowError ("Address argument must be a string");
		NanReturnValue (args.This ());
	}

	if (! args[1]->IsUint32 ()) {
		NanThrowError ("Port argument must be an unsigned integer");
		NanReturnValue (args.This ());
	}

	struct sockaddr_in serv_addr;
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	int portno = args[1]->ToInt32 ()->Value ();
	serv_addr.sin_family = AF_INET;
	NanAsciiString address (args[0]);
	if (inet_pton(AF_INET, *address, &(serv_addr.sin_addr)) == 0) {
		NanThrowError ("Adress must be a valid one");
		NanReturnValue (args.This ());
	}
	serv_addr.sin_port = htons(portno);

	int yes = 1;
	if (setsockopt(socket->poll_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		NanThrowError ("setsockopt() error");
		NanReturnValue (args.This ());
	}

	if (::bind(socket->poll_fd_, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		NanThrowError ("bind() error");
		NanReturnValue (args.This ());
	}

	NanReturnValue (args.This ());
}

NAN_METHOD(SocketWrap::Send) {
	NanScope();
	SocketWrap* socket = SocketWrap::Unwrap<SocketWrap> (args.This ());
	Local<Object> buffer;
	uint32_t offset;
	uint32_t length;
	int rc;
	char *data;

	if (args.Length () < 5) {
		NanThrowError ("Five arguments are required");
		NanReturnValue (args.This ());
	}

	if (! node::Buffer::HasInstance (args[0])) {
		NanThrowError ("Buffer argument must be a node Buffer object");
		NanReturnValue (args.This ());
	}

	if (! args[1]->IsUint32 ()) {
		NanThrowError ("Offset argument must be an unsigned integer");
		NanReturnValue (args.This ());
	}

	if (! args[2]->IsUint32 ()) {
		NanThrowError ("Length argument must be an unsigned integer");
		NanReturnValue (args.This ());
	}

	if (! args[3]->IsString ()) {
		NanThrowError ("Address argument must be a string");
		NanReturnValue (args.This ());
	}

	if (! args[4]->IsFunction ()) {
		NanThrowError ("Callback argument must be a function");
		NanReturnValue (args.This ());
	}

	rc = socket->CreateSocket ();
	if (rc != 0) {
		NanThrowError (raw_strerror (errno));
		NanReturnValue (args.This ());
	}

	buffer = args[0]->ToObject ();
	offset = args[1]->ToUint32 ()->Value ();
	length = args[2]->ToUint32 ()->Value ();
	NanAsciiString address (args[3]);

	data = node::Buffer::Data (buffer) + offset;

	if (socket->family_ == AF_INET6) {
		struct sockaddr_in6 addr;

		uv_ip6_addr (*address, 0, &addr);

		rc = sendto (socket->poll_fd_, data, length, 0,
				(struct sockaddr *) &addr, sizeof (addr));
	} else {
		struct sockaddr_in addr;

		uv_ip4_addr (*address, 0, &addr);

		rc = sendto (socket->poll_fd_, data, length, 0,
				(struct sockaddr *) &addr, sizeof (addr));
	}

	if (rc == SOCKET_ERROR) {
		NanThrowError (raw_strerror (SOCKET_ERRNO));
		NanReturnValue (args.This ());
	}

	Local<Function> cb = Local<Function>::Cast (args[4]);
	const unsigned argc = 1;
	Local<Value> argv[argc];
	argv[0] = NanNew<Number> (rc);
	cb->Call (NanGetCurrentContext()->Global (), argc, argv);

	NanReturnValue (args.This ());
}

NAN_METHOD(SocketWrap::SetOption) {
	NanScope();
	SocketWrap* socket = SocketWrap::Unwrap<SocketWrap> (args.This ());

	if (args.Length () < 3) {
		NanThrowError ("Three or four arguments are required");
		NanReturnValue (args.This ());
	}

	if (! args[0]->IsNumber ()) {
		NanThrowError ("Level argument must be a number");
		NanReturnValue (args.This ());
	}

	if (! args[1]->IsNumber ()) {
		NanThrowError ("Option argument must be a number");
		NanReturnValue (args.This ());
	}

	int level = args[0]->ToInt32 ()->Value ();
	int option = args[1]->ToInt32 ()->Value ();
	SOCKET_OPT_TYPE val = NULL;
	unsigned int ival = 0;
	SOCKET_LEN_TYPE len;

	if (args.Length () > 3) {
		if (! node::Buffer::HasInstance (args[2])) {
			NanThrowError ("Value argument must be a node Buffer object if length is provided");
	    	NanReturnValue (args.This ());
		}

		Local<Object> buffer = args[2]->ToObject ();
		val = node::Buffer::Data (buffer);

		if (! args[3]->IsInt32 ()) {
			NanThrowError ("Length argument must be an unsigned integer");
	    	NanReturnValue (args.This ());
		}

		len = args[3]->ToInt32 ()->Value ();

		if (len > node::Buffer::Length (buffer)) {
			NanThrowError ("Length argument is larger than buffer length");
	    	NanReturnValue (args.This ());
		}
	} else {
		if (! args[2]->IsUint32 ()) {
			NanThrowError ("Value argument must be a unsigned integer");
	    	NanReturnValue (args.This ());
		}

		ival = args[2]->ToUint32 ()->Value ();
		len = 4;
	}

	int rc = setsockopt (socket->poll_fd_, level, option,
			(val ? val : (SOCKET_OPT_TYPE) &ival), len);

	if (rc == SOCKET_ERROR) {
		NanThrowError (raw_strerror (SOCKET_ERRNO));
		NanReturnValue (args.This ());
	}

	NanReturnValue (args.This ());
}

static void IoEvent (uv_poll_t* watcher, int status, int revents) {
	SocketWrap *socket = static_cast<SocketWrap*>(watcher->data);
	socket->HandleIOEvent (status, revents);
}

}; /* namespace raw */

#endif /* RAW_CC */
