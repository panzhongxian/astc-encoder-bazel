import os
import uuid
from flask import Flask, flash, request, redirect, url_for, send_file
from werkzeug.utils import secure_filename
import ctypes

app = Flask(__name__)
app.secret_key = "super secret key"

SO_MODE_CTYPES = 0
SO_MODE_MODULE = 1

astc_encoder = None


class AstcEncoder():
    def __init__(self, mode=SO_MODE_CTYPES):
        if mode == SO_MODE_CTYPES:
            libname = "libastc-encoder.so"
            ctypes.CDLL(libname, mode=ctypes.RTLD_GLOBAL)
            libname = "libastc_wrapper.so"
            c_lib = ctypes.CDLL(libname)
            self._astc_compress_and_compare = lambda *args: c_lib.c_astc_compress_and_compare(
                *[str.encode(arg) for arg in args])
        elif mode == SO_MODE_MODULE:
            import astc
            self._astc_compress_and_compare = astc.astc_compress_and_compare
        else:
            raise RuntimeError("Invalid SO_MODE")

    def astc_compress_and_compare(self,
                                  uncompressed_file_path,
                                  color_profile="l",
                                  block="8x8",
                                  quality="medium"):
        base_path = uncompressed_file_path.rsplit('.', 1)[0]
        compressed_file_path = base_path + '.astc'
        decompressed_file_path = base_path + '.tga'
        self._astc_compress_and_compare(color_profile, \
            uncompressed_file_path, \
            compressed_file_path,\
            decompressed_file_path, \
            block,\
            quality)


def allowed_file(filename):
    ALLOWED_EXTENSIONS = {'png', 'jpg', 'jpeg'}
    return '.' in filename and \
                       filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS


@app.route('/astc-encoder/<action>', methods=['POST'])
def upload_file(action):
    if action not in ["encode", "preview"]:
        return "Please choose action from [encode, preview]"

    # check if the post request has the file part
    if 'file' not in request.files:
        return 'No file part'
    file = request.files['file']
    if not file or file.filename == '':
        return 'No selected file'
    if not allowed_file(file.filename):
        return 'Unsupported picture format'

    color_profile = request.args.get('color-profile', 'l')
    block = request.args.get('block', '8x8')
    quality = request.args.get('quality', 'medium')
    base_filename = secure_filename(file.filename).rsplit('.', 1)[0]
    filename = str(uuid.uuid4()) + '.' + secure_filename(file.filename)

    uncompressed_file_path = os.path.join("/tmp/flask-uploads", filename)
    compressed_file_path = uncompressed_file_path.rsplit('.', 1)[0] + '.astc'
    decompressed_file_path = uncompressed_file_path.rsplit('.', 1)[0] + '.tga'

    file.save(uncompressed_file_path)

    astc_encoder.astc_compress_and_compare(uncompressed_file_path,
                                           color_profile, block, quality)

    if action == "encode":
        return send_file(compressed_file_path,
                         attachment_filename=compressed_file_path.split(
                             os.sep)[-1])
    elif action == "preview":
        return send_file(decompressed_file_path,
                         attachment_filename=compressed_file_path.split(
                             os.sep)[-1])
    return ""


if __name__ == '__main__':
    astc_encoder = AstcEncoder(SO_MODE_CTYPES)  # or use SO_MODE_MODULE
    app.debug = True
    app.run(host='0.0.0.0', port=5000)
